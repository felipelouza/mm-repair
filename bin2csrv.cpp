/*  bin2csrv[if]
    (here and in the following [if] stands for either nothing, i or f)
    Convert a binary file contaning int32/float/double entries into the 
      CSRV format (.vc & .[if]val files) 
    or the 
      DRV  format (.dv & .[if]val files)
    In the CSRV format we only store nonzero entries and each entry
    is represented by an id identifying the value in the .[if]val file
    and the column number (hence the .vc extension)
    In the DRV format we store zero and nonzero entries and each entry is
    represented by an id identifying it in the .[if]val file 
    (hence the .dv extension) 
    In both formats id's are different from zero and the zero value is used 
    to mark the end of a matrix row 

    This source file is compiled in 3 versions:
      bin2csrv  (default),     matrix entries are 8 byte floats (eg doubles) 
      bin2csrvi (Typecode==1), matrix entries are 4 byte ints 
      bin2csrvf (Typecode==2), matrix entries are 4 byte floats 
    By default the output values stored in the [if]val file are in the same 
    type as in the input file, if option -d is used, output values are 
    stored as doubles. The extension [if]val is chosen according to the 
    type of the file values.  
    The option -c can be used to specify that the input values are complex
    with the real and immaginary parts either int/float or double.
    The type of the matrix entries does not affect the format of the 
    .vc/dv file  
    */
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

#if Typecode==1  // int32_t    
#define Type int32_t
#define valext ".ival"
#elif Typecode==2 // float
#define Type float
#define valext ".fval"
#else            // default type is double 
#define Type double
#define valext ".val"
#endif


// bit representing types of input/output values 
// and general options of the algorithm 
//#define INT32_OUTPUT 1
//#define FLOAT_OUTPUT 2
#define COMPLEX_INPUT 4
#define NO_COL_ID 8
#define DOUBLE_OUTPUT 16



static void usage_and_exit(char *name)
{
    fprintf(stderr,"Usage:\n\t  %s [options] matrix rows cols\n",name);
    fprintf(stderr,"\t\t-b num         number of row blocks, def. 1\n");    
    fprintf(stderr,"\t\t-c             input are complex numbers\n");
    fprintf(stderr,"\t\t-d             save matrix entries as doubles\n");
    fprintf(stderr,"\t\t-n             don't store col id (drv format, debug only)\n");
    fprintf(stderr,"\t\t-v             verbose\n");
    fprintf(stderr,"The option -c can be combined with -d\n\n");
    exit(1);
}

// write error message and exit
static void quit(const char *s)
{
  if(errno==0) fprintf(stderr,"%s\n",s);
  else  perror(s);
  exit(1);
}

// number of bits to represent longs up to n
int bits (unsigned long n) {
  int b=1;
  while (n>1) { n>>=1; b++; }
  return b;
}


// write a x as an int32/float/double in binary format  
// if there is loss of information a warning is sent to stderr
void write_bin(Type x, int out_type, FILE *f)
{
  int e;
  if(out_type&DOUBLE_OUTPUT) {
    double xd = x;
    e = fwrite(&xd,sizeof(xd),1,f);
    if(e!=1) 
      quit("Error writing to binary .val file (DOUBLE_OUTPUT)");
  }
  else {
    e = fwrite(&x,sizeof(x),1,f);
    if(e!=1) 
      quit("Error writing to binary " valext " file");
  }
}


// provide hash function for a complex<Type> 
struct ComplexHasher
{
  size_t operator()(const std::complex<Type>& k) const
  {
    using std::hash;

    return ((hash<Type>()(k.real())
             ^ (hash<Type>()(k.imag()) << 1)) >> 1);
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
  while ((c=getopt(argc, argv, "b:cdnv")) != -1) {
    switch (c) 
      {
      case 'v':
        verbose++; break;
      case 'c':
         vtype |= COMPLEX_INPUT; break;
      case 'd':
         vtype |= DOUBLE_OUTPUT; break;
      case 'n':
         vtype |= NO_COL_ID; break;
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


  // virtually get rid of options from the command line 
  optind -=1;
  if (argc-optind != 4) usage_and_exit(argv[0]); 
  argv += optind; argc -= optind;
  
  // ----------- read and check # rows and cols 
  rows  = atoi(argv[2]);
  if(rows<1) quit("Invalid number of rows");
  cols  = atoi(argv[3]);
  if(cols<1) quit("Invalid number of columns");
  size_t rowsize = cols*(vtype&COMPLEX_INPUT?2:1);


  if(nblocks>rows) quit("Too many row blocks!");
  // compute size of each block 
  int block_size = (rows+nblocks-1)/nblocks;
  fprintf(stderr,"Block size: %d\n", block_size);

  // open input file and check number of rows/cols
  FILE *f = fopen(argv[1],"r");
  if(f==NULL) quit("Cannot open infile");
  // check input file size 
  if(fseek(f,0,SEEK_END)!=0) quit("Cannot seek input file");
  size_t fsize = ftell(f);
  if(fsize<0) quit("Cannot tell input file size");
  if(fsize=!sizeof(Type)*rowsize*cols) quit("Invalid input file size");
  rewind(f);
  // open output .val file
  strncpy(fname,argv[1],PATH_MAX);
  strncat(fname,DOUBLE_OUTPUT ? ".val" : valext, PATH_MAX - 10);
  FILE *fval = fopen(fname,"w");
  if(fval==NULL) quit("Cannot open values file for writing");

  // init counters
  int  wr = 0;  // number of written rows
  unsigned long    nonz = 0;      // total number of processed values (nonzeros for crsv format)  
  unsigned long   dnonz = 0;      // distinct values in .val file (nonzeros for crsv format)  
  unsigned long maxcode = 0;      // largest code in a .vc/.dv file
  // dictionaty for storing input values
  std::unordered_map<Type,unsigned long> values; // dictionary of distinct nonzero
  Type v;   // values read from file
  // same for complex values
  std::unordered_map<std::complex<Type>,unsigned long, ComplexHasher> covalues; // dictionary of distinct nonzero
  std::complex<Type> cov;
  Type re,im;
  // extension of the matrix file .vc or .dv
  const char *mext = (vtype &NO_COL_ID) ? ".dv" : ".vc";
  // array cointaining a single row of the matrix
  Type *row = new Type[rowsize];

  // main loop reading binary file 
  size_t n=0;
  char *buffer=NULL;
  for(int bn=0;bn<nblocks;bn++) {
    if(nblocks==1) snprintf(fname,PATH_MAX,"%s%s",argv[1],mext);
    else snprintf(fname,PATH_MAX,"%s.%d.%d%s",argv[1],nblocks,bn,mext);
    FILE *fvc = fopen(fname,"w");
    if(fvc==NULL) quit("Cannot open a .vc/.dv file");
    while(true) {
      // read binary file one row at a time
      size_t e = fread(row,sizeof(Type),rowsize,f);
      if(e!=rowsize) quit("Error reading input file");
      for(int c=0;c<cols;c++) {
        if(vtype&COMPLEX_INPUT) {
          cov.real(row[2*c]);
          cov.imag(row[2*c+1]);
          v= (cov.real()==0 and cov.imag()==0) ? 0 : 1; // v==0 iff cov==0
        }
        else v = row[c];
        // process element if nonzero or NO_COL_ID is set
        if(v!=0 or (vtype&NO_COL_ID)) { // process non zero value
          unsigned long id, code;
          nonz += 1;      // note: v could be 0 so nonz is technically the wrong name
          // get id of entry and possibly write new values to .val file
          if(!(vtype&COMPLEX_INPUT)) { // real entry
            if(values.count(v)==0) {
              id = values[v] = dnonz++;
              write_bin(v,vtype,fval);
            } else id = values[v];
          } else { // complex entry 
            if(covalues.count(cov)==0) {
              id = covalues[cov] = dnonz++;
              write_bin(cov.real(),vtype, fval);
              write_bin(cov.imag(),vtype, fval);
            } else id = covalues[cov];
          }
          // generate code and write it to .vc file 
          if (vtype&NO_COL_ID) code = id;
          else code = id*cols + c;
          code += 1;                // shift by 1 to allow code for endrow code 0 
          if (code>= 1ul<<30) 
            quit("Code larger than 2**30. We are in trouble");
          if (code>maxcode) maxcode=code;
          wcode = (uint32_t) code; // convert to 32 bit value
          if(fwrite(&wcode,sizeof(wcode),1,fvc)!=1) 
            quit("Error writing to .vc/.dv file");
        }
      }
      // end row
      wcode=0;
      if(fwrite(&wcode,sizeof(wcode),1,fvc)!=1) 
        quit("Error writing to .vc/.dv file");
      wr += 1;
      // if a block is full, stop and start the next 
      if ((wr%block_size) == 0) break; // end while true
    }
    fclose(fvc);
  }
  delete[] row; // deallocate row array
  fclose(fval);
  if(vtype&COMPLEX_INPUT) assert(dnonz==covalues.size());
  else assert(dnonz==values.size());
  if(wr!=rows)
    fprintf(stderr, "Warning! Written %d rows instead of %d\n", wr, rows);
  fprintf(stderr,"Elapsed time: %.0lf secs\n",(double) (time(NULL)-start_wc));  
  fprintf(stderr,"Number of stored values: %ld   Stored ratio: %.4f\n", nonz, ((double) nonz/(wr*cols)));  
  fprintf(stderr, "%zd distinct values in .[if]val file (nonzeros in crsv format) \n", dnonz);
  fprintf(stderr,"Largest codeword: %lu   Bits x codeword: %d\n", maxcode, bits(maxcode));
  fprintf(stderr,"==== Done\n");
  
  return EXIT_SUCCESS;
}
