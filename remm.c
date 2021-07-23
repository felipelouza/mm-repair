/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * ReMatrix
 * 
 * test multiplication on repair compressed matrices
 * Given a matrix M and a vector x computes y=Mx and z^T = y^T M
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifdef CSR_MATRIX
#include "csrmatrix.h"
#else
#include "rematrix.h"
#endif
#ifdef MALLOC_COUNT
#include "mc/malloc_count.h"
#endif
#include <time.h>
#ifdef DETAILED_TIMING
#include <sys/times.h>
#endif

static void usage_and_exit(char *name)
{
    fprintf(stderr,"Usage:\n\t  %s [options] matrix rows cols xvector\n",name);
    fprintf(stderr,"\t\t-v             verbose\n");
    fprintf(stderr,"\t\t-n mul         number of multiplications, def. 1\n");
    fprintf(stderr,"\t\t-e einfile     store computed eigenvalue in this file\n");
    fprintf(stderr,"\t\t-y yvector     store y-vector in this file\n");
    fprintf(stderr,"\t\t-z zvector     store z-vector in this file\n\n");
    exit(1);
}


int main (int argc, char **argv) { 
  extern char *optarg;
  extern int optind, opterr, optopt;
  int verbose=0;
  FILE *f;
  int rows,cols,c,iter=1;
  xmatval lambda;
  char *ein_filename= NULL;
  char *yvec=NULL, *zvec=NULL;
  time_t start_wc = time(NULL);
  #ifdef DETAILED_TIMING
  struct tms ignored;
  clock_t t1,t2,t3;
  long m1=0,m2=0;
  #endif
  
  /* ------------- read options from command line ----------- */
  opterr = 0;
  while ((c=getopt(argc, argv, "e:n:y:z:v")) != -1) {
    switch (c) 
      {
      case 'v':
        verbose++; break;
      case 'n':
        iter=atoi(optarg); break;
      case 'e':
        ein_filename=optarg; break;
      case 'y':
        yvec=optarg; break;
      case 'z':
        zvec=optarg; break;
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
  if(iter<1) {
    fprintf(stderr,"Error! Option -n must be at least one\n");
    usage_and_exit(argv[0]);
  }  
  // virtually get rid of options from the command line 
  optind -=1;
  if (argc-optind != 5) usage_and_exit(argv[0]); 
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
  
  // compute products
  vector *y = vector_create();
  vector *z = vector_create();
  remat_mult(m,x,y);    // y = Mx
  remat_left_mult(y,m,z);   // z = y^t M
  #ifdef DETAILED_TIMING
  t3 = times(&ignored);
  #endif
  for(int i=1;i<iter;i++) {
    #ifdef DETAILED_TIMING
    t1 = t3;
    #endif 
    memcpy(x->v,z->v,sizeof(matval)*cols);  // copy z entries to x 
    lambda = vector_normalize(x); 
    remat_mult(m,x,y);
    #ifdef DETAILED_TIMING
    t2 = times(&ignored);
    m1 += (t2-t1);
    #endif
    remat_left_mult(y,m,z);
    #ifdef DETAILED_TIMING
    t3 = times(&ignored);
    m2 += (t3-t2);
    #endif
  }
  // last eigenvalue approximation
  if(verbose)
    fprintf(stderr, "Eigenvalue approximation after %d iterations: %lf\n",iter,(double) lambda);
  if(ein_filename!=NULL) {
    FILE *f = fopen(ein_filename,"wb");
    double ein = (double) lambda;
    if(fwrite(&ein,sizeof(double),1,f)!=1)
      die("Eigenvalue write error");
    fclose(f);
  }
  // --- open output y vector file
  if(yvec!=NULL) { 
    f = fopen (yvec,"w");
    if (f == NULL) die("Cannot open y output vector file");
    size_t e = fwrite(y->v,sizeof(matval),y->size,f);
    if(e!=y->size) die("Cannot write to y output file");
    if(fclose(f)!=0) die("Cannot close y output file");
  }
  // --- open output z vector file
  if(zvec!=NULL) { 
    f = fopen (zvec,"w");
    if (f == NULL) die("Cannot open z output vector file");
    size_t e = fwrite(z->v,sizeof(matval),z->size,f);
    if(e!=z->size) die("Cannot write to z output file");
    if(fclose(f)!=0) die("Cannot close z output file");
  }
  
  // destroy 
  vector_destroy(z);
  vector_destroy(y);
  vector_destroy(x);
  remat_destroy(m);
  #ifdef MALLOC_COUNT
    fprintf(stderr,"Peak memory allocation: %zu bytes, %.4lf bytes/entries\n",
           malloc_count_peak(), (double)malloc_count_peak()/(rows*cols));
  #endif
  #ifdef DETAILED_TIMING
  fprintf(stderr,"Average mult time (secs) Ax: %lf  xA: %lf\n", ((double)m1/iter)/sysconf(_SC_CLK_TCK), ((double)m2/iter)/sysconf(_SC_CLK_TCK));
  #endif 
  printf("Elapsed time: %.0lf secs\n",(double) (time(NULL)-start_wc));  
  return 0;
}

