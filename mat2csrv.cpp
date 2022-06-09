#include <cassert>
#include <iostream>
// #include <vector>
#include <unordered_map>
#include <map>
#include <cstring>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <complex>
#include <array>

// bit representing types of input/output values 
#define INT32_OUTPUT 1
#define FLOAT_OUTPUT 2
#define COMPLEX_INPUT 4


static void usage_and_exit(char *name)
{
    fprintf(stderr,"Usage:\n\t  %s [options] matrix rows cols\n",name);
    fprintf(stderr,"\t\t-b num         number of row blocks, def. 1\n");
    fprintf(stderr,"\t\t-c             input are complex numbers\n");
    fprintf(stderr,"\t\t-f             save entries as floats\n");
    fprintf(stderr,"\t\t-i             save entries as int32s\n");
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

// number of bits to represent longs up to n
int bits (unsigned long n) {
  int b=1;
  while (n>1) { n>>=1; b++; }
  return b;
}


// write a double x as a int32/float/double in binary format  
void write_bin(double x, int out_type, FILE *f)
{
  int e;
  
  if(out_type & INT32_OUTPUT) {
    int32_t y = (int32_t) lround(x);
    e = fwrite(&y,sizeof(y),1,f);
  }
  else if(out_type & FLOAT_OUTPUT) {
    float y = (float) x;
    e = fwrite(&y,sizeof(y),1,f);
  }
  else // double
    e = fwrite(&x,sizeof(x),1,f);
    
  if(e!=1) quit("Error writing to binary file");
} 


// provide hash function for a complex<double> 
struct ComplexHasher
{
  size_t operator()(const std::complex<double>& k) const
  {
    using std::hash;

    return ((hash<double>()(k.real())
             ^ (hash<double>()(k.imag()) << 1)) >> 1);
  }
};


int main (int argc, char **argv) { 
  extern char *optarg;
  extern int optind, opterr, optopt;
  int verbose=0,vtype=0;
  int c,rows,cols,nblocks=1;
  char fname[PATH_MAX];
  time_t start_wc = time(NULL);
  uint32_t wcode;  // int written to the .vc files

  /* ------------- read options from command line ----------- */
  opterr = 0;
  while ((c=getopt(argc, argv, "b:cfiv")) != -1) {
    switch (c) 
      {
      case 'v':
        verbose++; break;
      case 'f':
        vtype |= FLOAT_OUTPUT; break;
      case 'i':
        vtype |= INT32_OUTPUT; break;
      case 'c':
         vtype |= COMPLEX_INPUT; break;
      case 'b':
        nblocks=atoi(optarg); break;
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
  // check command line
  if(nblocks < 1) {
    fprintf(stderr,"Error! Option -b must be at least one\n");
    usage_and_exit(argv[0]);
  }  
  if( (vtype&INT32_OUTPUT) && (vtype&FLOAT_OUTPUT) ) {
    fprintf(stderr,"Error! Options -f and -i are mutually exclusive\n");
    usage_and_exit(argv[0]);
  }  

  // virtually get rid of options from the command line 
  optind -=1;
  if (argc-optind != 4) usage_and_exit(argv[0]); 
  argv += optind; argc -= optind;
  
  // ----------- read and check # rows and cols 
  rows  = atoi(argv[2]);
  if(rows<1) quit("Invalid number of rows");
  cols  = atoi(argv[3]);
  if(cols<1) quit("Invalid number of columns");
  
  if(nblocks>rows) quit("Too many row blocks!");
  // compute size of each block 
  int block_size = (rows+nblocks-1)/nblocks;
  fprintf(stderr,"Block size: %d\n", block_size);

  // open file and init
  FILE *f = fopen(argv[1],"r");
  if(f==NULL) quit("Cannot open infile");
  strncpy(fname,argv[1],PATH_MAX);
  strcat(fname,".val");
  FILE *fval = fopen(fname,"w");
  if(fval==NULL) quit("Cannot open valfile");
  // init counters
  int  r = 0;   // number of read rows
  int  wr = 0;  // number of written rows
  unsigned long    nonz = 0;      // total number of nonzeros  
  unsigned long   dnonz = 0;      // distinct nonzeros  
  unsigned long maxcode = 0;      // largest code in a .vc file
  // variables for real input values
  std::unordered_map<double,unsigned long> values; // dictionary of distinct nonzero
  double v;   // values read from file
  // same for complex values
  std::unordered_map<std::complex<double>,unsigned long, ComplexHasher> covalues; // dictionary of distinct nonzero
  std::complex<double> cov;
  double re,im;
  
  // main loop reading csv file 
  size_t n=0;
  char *buffer=NULL;
  for(int bn=0;bn<nblocks;bn++) {
    if(nblocks==1) snprintf(fname,PATH_MAX,"%s.vc",argv[1]);
    else snprintf(fname,PATH_MAX,"%s.%d.%d.vc",argv[1],nblocks,bn);
    FILE *fvc = fopen(fname,"w");
    if(fvc==NULL) quit("Cannot open a .vc file");
    while(true) {
      // read csv file
      int e = getline(&buffer,&n,f);
      if(e<0) {
        if(ferror(f)) quit("Error reading input file");
        break;
      }
      r += 1;
      if (wr>= rows) {
        fprintf(stderr,"Warning: more matrix rows than expected\n");
        break;
      } 
      // convert text numerical values
      char *s = strtok(buffer,",");
      // loop over entries in current row
      for(int c=0;c<cols;c++) {
        // convert s into double and check
        if(s==NULL) {
          fprintf(stderr,"Missing value in row %d column %d (one based)\n",r,c+1);
          quit("Check input file");
        }
        char *tmp;
        // ---- read a value from file either a double or a complex<double>
        if(vtype&COMPLEX_INPUT) { // complex value
          int e = sscanf(s," (%lf%lfj)",&re,&im);
          if(e!=2) {
            fprintf(stderr,"Invalid or missing complex entry in row %d column %d (one based)\n",r,c+1);
            quit("Check input file");
          }
          v= (re==0 and im ==0) ? 0 : 1; // v==0 iff cov==0
          cov.real(re); cov.imag(im);    // init cov          
        }
        else { // plain real value
          v = strtod(s,&tmp);
          if(tmp==s) {
            fprintf(stderr,"Missing value in row %d column %d (one based)\n",r,c+1);
            quit("Check input file");
          }
        }
        // ----- process element if nonzero 
        if(v!=0) { // process non zero value
          unsigned long id, code;
          nonz += 1;
          // get id of entry and possibly write new values to .val file
          if(!(vtype&COMPLEX_INPUT)) { // real entry
            if(values.count(v)==0) {
              id = values[v] = dnonz++;
              write_bin(v,vtype,fval);
            } else id = values[v];
          } else { // complex entry 
            if(covalues.count(cov)==0) {
              id = covalues[cov] = dnonz++;
              write_bin(cov.real(),vtype,fval);
              write_bin(cov.imag(),vtype,fval);
            } else id = covalues[cov];
          }
          // generate code and write it to .vc file 
          code = id*cols + c;
          code += 1;                // shift by 1 to allow code for endrow code 0 
          if (code>= 1ul<<30) 
            quit("Code larger than 2**30. We are in trouble");
          if (code>maxcode) maxcode=code;
          wcode = (uint32_t) code; // convert to 32 bit value
          if(fwrite(&wcode,sizeof(wcode),1,fvc)!=1) 
            quit("Error writing to .vc file");
        }
        s = strtok(NULL,","); // scan next value
      }
      if(s!=NULL) {
        fprintf(stderr,"Extra value in row %d (one based)\n",r);
        quit("Check input file");
      }
      // end row
      wcode=0;
      if(fwrite(&wcode,sizeof(wcode),1,fvc)!=1) 
        quit("Error writing to .vc file");
      wr += 1;
      // if a block is full, stop and start the next 
      if ((wr%block_size) == 0) break; // end while true
    }
    fclose(fvc);
  }
  fclose(fval);
  if(vtype&COMPLEX_INPUT) assert(dnonz==covalues.size());
  else assert(dnonz==values.size());
  if(wr!=rows)
    fprintf(stderr, "Warning! Written %d rows instead of %d\n", wr, rows);
  fprintf(stderr,"Elapsed time: %.0lf secs\n",(double) (time(NULL)-start_wc));  
  fprintf(stderr,"Number of nonzeros: %ld Nonzero ratio: %.4f\n", nonz, ((double) nonz/(wr*cols)));  
  fprintf(stderr, "%zd distinct nonzeros values\n", dnonz);
  fprintf(stderr,"Largest codeword: %lu bits: %d\n", maxcode, bits(maxcode));
  fprintf(stderr,"==== Done\n");
  
  return EXIT_SUCCESS;
}
