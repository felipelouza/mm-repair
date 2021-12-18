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
#elif defined(USE_INTVEC) || defined(USE_ANSIV)
#include "rematrix.hpp"
#else
#include "rematrix.h"
#endif
#ifdef MALLOC_COUNT
#include "tools/malloc_count.h"
#endif
#include <time.h>
#ifdef DETAILED_TIMING
#include <sys/times.h>
#endif
#include <pthread.h>
#include <semaphore.h>
#include "tools/xerrors.h"

// input/output data for each thread 
typedef struct {
  rematrix *m;   // matrix block
  vector *rv;    // row vector    (same size as a matrix row)
  vector *cv;    // column vector (      "               column)
  int op;        // operation to be performed
  sem_t *in;    // semaphore for input shared with the main thread
  sem_t *out;   // semaphore for output shared with the main thread
} tdata;


static void usage_and_exit(char *name)
{
    fprintf(stderr,"Usage:\n\t  %s [options] matrix rows cols xvector\n",name);
    fprintf(stderr,"\t\t-v             verbose\n");
    fprintf(stderr,"\t\t-b num         number of row blocks, def. 1\n");
    fprintf(stderr,"\t\t-n mul         number of multiplications, def. 1\n");
    fprintf(stderr,"\t\t-e einfile     store computed eigenvalue in this file\n");
    fprintf(stderr,"\t\t-y yvector     store y-vector in this file\n");
    fprintf(stderr,"\t\t-z zvector     store z-vector in this file\n\n");
    exit(1);
}

static rematrix **remat_create_multipart(int, int,const char *, int blocks);
static void remat_destroy_multipart(rematrix **b,int n);
static void *block_main(void *v);
static void remat_left_mult_mth(vector *yv, rematrix *m,vector *x, tdata *td, int n);
static void remat_mult_mth(rematrix *m,vector *x,vector *yv, tdata *td, int n);


int main (int argc, char **argv) { 
  extern char *optarg;
  extern int optind, opterr, optopt;
  int verbose=0;
  FILE *f;
  int rows,cols,c,iter=1,nblocks=1;
  xmatval lambda=0;
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
  while ((c=getopt(argc, argv, "e:n:b:y:z:v")) != -1) {
    switch (c) 
      {
      case 'v':
        verbose++; break;
      case 'n':
        iter=atoi(optarg); break;
      case 'b':
        nblocks=atoi(optarg); break;
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
  if(iter<1 || nblocks < 1) {
    fprintf(stderr,"Error! Options -b and -n must be at least one\n");
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
  
  // ------------ read matrix or row blocks
  rematrix *m = NULL;
  rematrix **rblocks = NULL; 
  if(nblocks==1)
    m = remat_create(rows,cols,argv[1]); 
  else 
    rblocks = remat_create_multipart(rows,cols,argv[1],nblocks);
    
  // ------------ read input vector
  f = fopen(argv[4],"rb");
  if(f==NULL) die("Cannot open input vector file");
  vector *x = vector_create();
  x->v = read_vals(f,&x->size);
  if(x->size!=cols) die("Input vector size should be equal to # of columns");
  fclose(f);
  
  // create auxiliary vectors 
  vector *y = vector_create();
  vector_set_zero(y,rows);
  vector *z = vector_create();
  vector_set_zero(z,cols);

  // data structures for multithread computation (nblocks>1)
  vector *yv = NULL;  // array of subvectors
  tdata td[nblocks];
  pthread_t t[nblocks];
  sem_t tsem_in[nblocks];
  sem_t tsem_out[nblocks];
  
  // initialize thread data
  if(nblocks>1) {
    // yv entries coincide with those of y  
    yv = vector_split(y,nblocks);
    for(int i=0;i<nblocks;i++) {
      td[i].m = rblocks[i];
      td[i].in = &tsem_in[i];
      td[i].out = &tsem_out[i];
      xsem_init(&tsem_in[i],0,0,__LINE__,__FILE__);
      xsem_init(&tsem_out[i],0,0,__LINE__,__FILE__);
      xpthread_create(&t[i],NULL,&block_main,&td[i],__LINE__,__FILE__);
    }
  }
    
  // compute products  
  if(nblocks==1) {
    remat_mult(m,x,y);    // y = Mx
    remat_left_mult(y,m,z);   // z = y^t M
  }
  else {
    remat_mult_mth(m,x,yv,td,nblocks);    // y = Mx
    remat_left_mult_mth(yv,m,z,td,nblocks);   // z = y^t M
  }
  #ifdef DETAILED_TIMING
  t3 = times(&ignored);
  #endif
  for(int i=1;i<iter;i++) {
    #ifdef DETAILED_TIMING
    t1 = t3;
    #endif 
    memcpy(x->v,z->v,sizeof(matval)*cols);  // copy z entries to x 
    lambda = vector_normalize(x); 
    if(nblocks==1) remat_mult(m,x,y);
    else remat_mult_mth(m,x,yv,td,nblocks);
    #ifdef DETAILED_TIMING
    t2 = times(&ignored);
    m1 += (t2-t1);
    #endif
    if(nblocks==1) remat_left_mult(y,m,z);
    else remat_left_mult_mth(yv,m,z,td,nblocks);
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
  if(nblocks==1) 
    remat_destroy(m);
  else {
    free(yv);
    remat_destroy_multipart(rblocks,nblocks);
    for(int i=0;i<nblocks;i++) {
      td[i].op = -1; // stop thread
      xsem_post(td[i].in,__LINE__,__FILE__);
      pthread_join(t[i],NULL);
      xsem_destroy(td[i].in,__LINE__,__FILE__);
      xsem_destroy(td[i].out,__LINE__,__FILE__);
    }
  }
  #ifdef MALLOC_COUNT
    fprintf(stderr,"Peak memory allocation: %zu bytes, %.4lf bytes/entries\n",
           malloc_count_peak(), (double)malloc_count_peak()/(rows*cols));
    fprintf(stderr,"Current memory allocation: %zu bytes\n", malloc_count_current());
  #endif
  #ifdef DETAILED_TIMING
  fprintf(stderr,"Average mult time (secs) Ax: %lf  xA: %lf\n", ((double)m1/iter)/sysconf(_SC_CLK_TCK), ((double)m2/iter)/sysconf(_SC_CLK_TCK));
  #endif 
  printf("Elapsed time: %.0lf secs\n",(double) (time(NULL)-start_wc));  
  return 0;
}


// function executed by each thread 
// wait on a semaphore for a ner operation to execute
// on its given matrix
static void *block_main(void *v)
{
  tdata *td = (tdata *) v;
  vector *auxrow = vector_create();
  vector_set_zero(auxrow,td->m->cols);  
  
  while(true) {
    // wait for input 
    xsem_wait(td->in,__LINE__,__FILE__);
    if(td->op<0) break;
    else if(td->op==0) { //left mult
      assert(td->cv!=NULL); // the input is a column vector
      if(td->rv==NULL) td->rv = auxrow;
      remat_left_mult(td->cv,td->m,td->rv);
    }
    else if(td->op==1) { //right mult
      assert(td->rv!=NULL); // the input is a row vector
      assert(td->cv!=NULL); // output must be given here 
      remat_mult(td->m,td->rv,td->cv);
    }
    else die("Unknown operation");
    // output ready 
    xsem_post(td->out,__LINE__,__FILE__);
  }
  vector_destroy(auxrow);
  return NULL;
}




// read matrix consisting of n blocks  
static rematrix **remat_create_multipart(int rows,int cols,const char *base, int n)
{
  assert(n>1); // there must be at least 2 blocks 
  
  rematrix **b = (rematrix **) malloc(n*sizeof *b);
  if(b==NULL) die("Not enough memory");
  int maxblock = (rows+n-1)/n;
  assert(maxblock>=1);
  
  char fname[PATH_MAX];
  int remaining = rows;
  for(int i=0;i<n;i++) {
    assert(remaining>0);
    snprintf(fname,PATH_MAX,"%s.%d.%d",base,n,i);
    int r = (remaining>maxblock? maxblock : remaining);
    assert(r>0);
    b[i] = remat_create(r,cols,fname);
    remaining -= r;
  }
  assert(remaining==0);
  return b;
}

static void remat_destroy_multipart(rematrix **b,int n)
{
  for(int i=0;i<n;i++)
    remat_destroy(b[i]);
  free(b);
}


static void remat_mult_mth(rematrix *m,vector *x,vector *yv, tdata *td, int n)
{
  // start the block multiplications
  for(int i=0;i<n;i++) {
    td[i].rv = x;      // input  
    td[i].cv = &yv[i]; // output here 
    td[i].op = 1;      // right mult
    xsem_post(td[i].in, __LINE__,__FILE__);
  }
  // wait for all blocks
  for(int i=0;i<n;i++)
    xsem_wait(td[i].out, __LINE__,__FILE__); 
}

static void remat_left_mult_mth(vector *yv, rematrix *m,vector *x, tdata *td, int n)
{
  // start the block multiplications
  for(int i=0;i<n;i++) {
    td[i].rv = NULL;     // output in the internal vector 
    td[i].cv = &yv[i];   // input 
    td[i].op = 0;        // left mult
    xsem_post(td[i].in, __LINE__,__FILE__);
  }
  // clean result vector
  vector_set_zero(x,x->size);
  // wait for all blocks and sum all the x vectors 
  for(int i=0;i<n;i++) {
    xsem_wait(td[i].out, __LINE__,__FILE__);
    assert(x->size==td[i].rv->size);
    for(int j=0;j<x->size;j++)
      x->v[j] += td[i].rv->v[j];
  } 
}

