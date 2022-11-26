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
    fprintf(stderr,"Usage:\n\t  %s [options] vcfile\n",name);
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
  long nonz=0;
  char fname[PATH_MAX];
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
  if (argc-optind != 2) usage_and_exit(argv[0]); 
  argv += optind; argc -= optind;
  
  

  // open file and init
  FILE *f = fopen(argv[1],"r");
  if(f==NULL) quit("Cannot open infile");
  // count number of occurrences of each values
  std::unordered_map<vc_t,uint32_t> occ;
  vc_t maxv=0;  // largest int in the .vc file: we keep it in the vc2 file
  while(true) {
    vc_t v;  // int from the .vc file
    size_t e = fread(&v,1,sizeof(v),f); // NOTE: this is the right way to check reading a vc_t
    if(e==sizeof(v)) {
      if(v!=0) {
        nonz++;             // count nonzero
        if(v>maxv) maxv=v;  // keep largest int
      }
      if(occ.count(v)==0) // new value
        occ[v]=1;
      else
        occ[v] += 1;     // update count
    }
    else if(ferror(f))
      quit("Error reading input file");
    else if(e!=0)      // read partial vc_t error! 
      quit("Invalid input file");
    else {
      assert(feof(f));
      break;             // e==0 and eof reached
    } 
  }
  // create files of singletons and multi-tons
  strncpy(fname,argv[1],PATH_MAX);
  strcat(fname,"1");
  FILE *fval1 = fopen(fname,"w");
  if(fval1==NULL) quit("Cannot open singletons valfile");
  strncpy(fname,argv[1],PATH_MAX);
  strcat(fname,"2");
  FILE *fval2 = fopen(fname,"w");
  if(fval2==NULL) quit("Cannot open multitons valfile");
  // split into singleton and multiton
  rewind(f);
  long singletons=0, multitons=0;
  while(true) {
    vc_t v;           // value from the .vc file
    uint32_t occv=0;  // occ of v, if v!=0
    size_t e = fread(&v,sizeof(v),1,f);
    if(e==1) {
      occv = occ[v];
      if(occv==0) quit("Unknown value: Something is seriously wrong");
      // update # single/multi -tons
      if(v!=0) { if(occv==1)  singletons++; else multitons++;}
      // write v to the correct file (in both if v==0)  
      if(v==0 || v==maxv || occv==1) {
        e = fwrite(&v,sizeof(v),1,fval1);
        if(e!=1) quit("Error writing to singletons valfile ");
      }
      if(v==0 || (occv>1 && v!=maxv)  ) {
        assert(v<maxv);
        e = fwrite(&v,sizeof(v),1,fval2);
        if(e!=1) quit("Error writing to multitons valfile ");
      }
    }
    else if(feof(f)==0)  // e==0 not at end of file: error! 
      quit("Error reading from input file");
    else
      break;             // e==0 and eof reached 
  }
  // done
  fclose(fval2);
  fclose(fval1);
  fclose(f);

  fprintf(stderr,"Elapsed time: %.0lf secs\n",(double) (time(NULL)-start_wc));  
  fprintf(stderr,"Number of rows %u\n", occ[0]);  
  fprintf(stderr,"Number of nonzeros: %ld\n", nonz);  
  fprintf(stderr, "%ld singleton nonzero values\n", singletons);
  fprintf(stderr, "%ld  multiton nonzero values\n", multitons);
  fprintf(stderr, "%ld  distinct nonzero values\n", singletons+multitons);
  fprintf(stderr,"==== Done\n");
  
  return EXIT_SUCCESS;
}
