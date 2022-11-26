#include <cassert>
#include <iostream>
#include <unordered_map>
#include <map>
#include <cstring>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <complex>
#include <array>
 
// integer unsigned type used to represent value/column values 
typedef uint32_t vc_t;

static void usage_and_exit(char *name)
{
    fprintf(stderr,"Usage:\n\t  %s [options] vcfile_in1 vcfile_in2 vcfile_out\n",name);
    fprintf(stderr,"\t\t-v             verbose\n\n");
    exit(1);
}

// write error message and exit
static void quit(const char *s)
{
  if(errno==0) 
     fprintf(stderr,"%s\n",s);
  else 
    perror(s);
  exit(1);
}


int main (int argc, char **argv) { 
  //extern char *optarg;
  extern int opterr, optopt, optind;
  int c,verbose=0;
  time_t start_wc = time(NULL);

  /* ------------- read options from command line ----------- */
  opterr = 0;
  while ((c=getopt(argc, argv, "b:cfiv")) != -1) {
    switch (c) 
      {
      case 'v':
        verbose++; break;
      case '?':
        fprintf(stderr,"Unknown option: %c\n", optopt);
        exit(1);
      }
  }
  if(verbose>0) {
    fputs("==== Command line:\n",stderr);
    for(int i=0;i<argc;i++)
     fprintf(stderr," %s",argv[i]);
    fputs("\n",stderr);  
  }
  // virtually get rid of options from the command line 
  optind -=1;
  if (argc-optind != 4) usage_and_exit(argv[0]); 
  argv += optind; argc -= optind;
  
  

  // open files and init
  FILE *fin[2];
  for(int j=0;j<2;j++) {
    fin[j] = fopen(argv[1+j],"rb");
    if(fin[j]==NULL) quit("Cannot open input file");
  }
  FILE *fout = fopen(argv[3],"wb");
  if(fout==NULL) quit("Cannot open infile");
  
  bool endfile[2] = {false,false};
  long rows=0;
  while(true) { 
    vc_t v; size_t e;
    rows++;
    for(int j=0;j<2;j++) { // read from fin[0] and fin[1]
      for(int i=0; ;i++) {        
        e = fread(&v,sizeof(v),1,fin[j]);
        if(e!=1) {
          if(ferror(fin[j])) quit("Read error");
          if(i!=0) quit("Input file non zero-terminated");
          endfile[j]=true;
          break;  // end of current file
        }
        if(v==0) break;  // end of current row
        e = fwrite(&v,sizeof(v),1,fout); // nonzero value must be copied to outfile
        if(e!=1) quit("Error writing to outfile");
      }
    } // both files have written their segment, is it over? 
    if(endfile[0] xor endfile[1]) // input files must end simultaneously
      quit("Input files with different number of rows");
    if(endfile[0] && endfile[1])
      break; // both files have ended exit loop
    v=0;     // write 0 to conclude missing row
    e = fwrite(&v,sizeof(v),1,fout);
    if(e!=1) quit("Error writing to outfile");
  }
  // done
  fclose(fout);
  fclose(fin[1]);
  fclose(fin[0]);

  fprintf(stderr,"Elapsed time: %.0lf secs\n",(double) (time(NULL)-start_wc));  
  fprintf(stderr,"Number of rows %ld\n", rows);  
  fprintf(stderr,"==== Done\n");
  
  return EXIT_SUCCESS;
}
