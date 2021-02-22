/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * ReMatrix
 * 
 * operations on repair compressed matrices
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
#include "rematrix.h"
#ifdef MALLOC_COUNT
#include "mc/malloc_count.h"
#endif

static void usage_and_exit(char *name)
{
    fprintf(stderr,"Usage:\n\t %s [-n mul] <matrix> rows cols <invector> <outvector> \n",name);
    fprintf(stderr,"\t\t-n mul         number of multiplications, def. 1\n\n");
    exit(1);
}


int main (int argc, char **argv) { 
  extern char *optarg;
  extern int optind, opterr, optopt;
  FILE *f;
  int rows,cols,c,iter=1;
  // ----------- check input
  fputs("==== Command line:\n",stderr);
  for(int i=0;i<argc;i++)
   fprintf(stderr," %s",argv[i]);
  fputs("\n",stderr);  
  
  /* ------------- read options from command line ----------- */
  opterr = 0;
  while ((c=getopt(argc, argv, "n:")) != -1) {
    switch (c) 
      {
      case 'n':
        iter=atoi(optarg); break;
      case '?':
        fprintf(stderr,"Unknown option: %c\n", optopt);
        exit(1);
      }
  }
  // check command line
  if(iter<1) {
    fprintf(stderr,"Error! Option -n must be at least one\n");
    usage_and_exit(argv[0]);
  }  
  // virtually get rid of options from the command line 
  optind -=1;
  if (argc-optind != 6) usage_and_exit(argv[0]); 
  argv += optind; argc -= optind;
  
  // ----------- read and check # rows and cols 
  rows  = atoi(argv[2]);
  if(rows<1) die("Invalid number of rows");
  cols  = atoi(argv[3]);
  if(cols<1) die("Invalid number of columns");
  // ------------ read input vector
  f = fopen(argv[4],"rb");
  if(f==NULL) die("Cannot open input vector file");
  vector *x = vector_create();
  x->v = read_vals(f,&x->size);
  if(x->size!=cols) die("Input vector size should be equal to # of columns");
  fclose(f);
  // ------------ read matrix
  rematrix *m = remat_create(rows,cols,argv[1]); 
  
  // compute product
  vector *y = vector_create();
  remat_mult(m,x,y);
  for(int i=1;i<iter;i++) {
    memcpy(x->v,y->v,sizeof(matval)*(rows<cols?rows:cols));
    vector_normalize(x); 
    remat_mult(m,x,y);
  }
  
  // --- open output vector file 
  f = fopen (argv[5],"w");
  if (f == NULL) die("Cannot open output vector file");
  size_t e = fwrite(y->v,sizeof(matval),y->size,f);
  if(e!=y->size) die("Cannot write to output file");
  if(fclose(f)!=0) die("Cannot close output file");
  
  // destroy 
  vector_destroy(y);
  vector_destroy(x);
  remat_destroy(m);
  #ifdef MALLOC_COUNT
    printf("Peak memory allocation: %zu bytes, %.4lf bytes/entries\n",
           malloc_count_peak(), (double)malloc_count_peak()/(rows*cols));
  #endif
  return 0;
}

