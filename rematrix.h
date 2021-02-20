/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * ReMatrix
 * 
 * operations on repair compressed matrices
 * 
 * Copyright (C) 2021-2099   giovanni.manzini@uniupo.it
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

// set to 1 to print a lot of debug information 
#define DEBUG 0


#ifndef INT_VALS 
typedef float   matval;  // type representing a matrix entry   
typedef double xmatval;  // type representing a matrix entry with larger precision   
#else
typedef int matval;     // type representing a matrix entry   
typedef int xmatval;    // type representing a matrix entry with larger precision   
#endif


// matrix represented as a re-pair grammat

typedef struct {
  int rows,cols;  // # rows and columns of input matrix 
  int Alpha;      // alphabet size of input matrix representation/smallest non terminal symbol 
  size_t NTnum;      // number of non terminals 
  int *NTrules;   // right hand side of non-terminal 
  matval *NTval;  // values associated to non-terminals (memory shared with NTrules)
  int *Cseq;      // array C of repair grammar
  size_t Clen;       // len of C array
  size_t Mnum;       // number of distinct non zero matrix values
  matval *Mval;   // set of distinct nonzero matrix values
  FILE *Cf;       // C file 
  FILE *Rf;       // R file  
} rematrix;  

typedef struct {
  matval *v;
  size_t size;
} vector;



// main prototypes
rematrix *remat_create(int r, int c, char *basename);
void remat_destroy(rematrix *v);
vector *vector_create();
void vector_destroy(vector *v);
void remat_mult(rematrix *m, vector *x, vector *y);
matval *read_vals(FILE *f, size_t* size);
xmatval decode_entry(int p, rematrix *m, vector *x);

// local functions 
static void die(const char *s);
static void fill_NTval(rematrix *m, vector *x, bool share);


rematrix *remat_create(int r, int c, char *basename)
{
  char fname[PATH_MAX];
  FILE *f; struct stat s;
  rematrix *m=malloc(sizeof(rematrix));
  if(m==NULL) die("Cannot alloc matrix");
  
  m->rows=r; m->cols=c;

  // ------------ read rules
  strcpy(fname,basename);
  strcat(fname,".il.R");
  if (stat (fname,&s) != 0)die("Cannot stat rules (.il.R) file");
  off_t len = s.st_size;
  m->Rf = fopen (fname,"rb");
  if (m->Rf == NULL) die("Cannot open rules (.il.R) file"); 

  // read alphabet size for the original input string 
  if (fread(&m->Alpha,sizeof(int),1,m->Rf) != 1)  
   die("Cannot read rules (.il.R) file (1)"); 
  m->NTnum = (len-sizeof(int))/(2*sizeof(int)); // number of non terminal (rules) 
  m->NTrules = (int *) malloc(m->NTnum*2*sizeof(int));
  if(m->NTrules==NULL) die("Cannot allocate R array");
  if(fread(m->NTrules,2*sizeof(int),m->NTnum,m->Rf)!=m->NTnum)
    die("Cannot read rules (.il.R) file (2)");
  // values are used only for right multiplication, no need to allocate now   
  m->NTval = NULL;
  
  // --- open and read C file
  strcpy(fname,basename);
  strcat(fname,".il.C");
  if (stat (fname,&s) != 0) die("Cannot stat .il.C file");
  m->Cf = fopen (fname,"r");
  if (m->Cf == NULL) die("Cannot open .il.C file");
  m->Clen = (s.st_size)/sizeof(int);
  m->Cseq = (int *) malloc(m->Clen*sizeof(int));
  if(fread(m->Cseq,sizeof(int),m->Clen,m->Cf)!=m->Clen)
   die("Cannot read .il.C files");
  
  // ------------ read matrix values 
  strcpy(fname,basename);
  strcat(fname,".val");
  f = fopen(fname,"rb");
  if(f==NULL) die("Cannot open matrix values (.val) file");
  m->Mval = read_vals(f,&m->Mnum);
  if(fclose(f)!=0) die("Error closing values (.val) file");

  return m;
}

void remat_mult(rematrix *m, vector *x, vector *y)
{
  // compute NT values according to x
  fill_NTval(m,x,false);
  // create y
  y->size = m->rows;
  y->v = (matval *) realloc(y->v,y->size*sizeof(matval));
   // --- compute output 
   int ywritten = 0;
   xmatval sum=0;
   for(size_t j=0; j < m->Clen; j++) {
     int i = m->Cseq[j];
     if(i>=m->Alpha) { // non terminal 
       if( (i = i-m->Alpha)>= (int)m->NTnum ) die("Illegal non terminal in C file");
       sum += m->NTval[i];
     }
     else if(i>=m->rows) {// terminal representing a matrix entry
       sum += decode_entry(i-m->rows,m,x);
     }
     else { // row completed
       if(i!=ywritten) die("Incorrect end of row separator");
       y->v[ywritten] = (matval) sum;
       sum = 0;
       if(++ywritten==m->rows) assert(j+1==m->Clen);
     }
  }
  assert(ywritten==m->rows);
  assert(sum==0);
}


void remat_destroy(rematrix *m)
{  
  free(m->Mval);
  // these are usually accessed seuqntially, so one could keep them on file
  if(m->Cseq) {free(m->Cseq); m->Cseq=NULL;}
  if(m->NTrules) {free(m->NTrules); m->NTrules=NULL;}
  if(m->NTval) {free(m->NTval); m->NTval=NULL;}

  // the files were left open in case their data have to be reloaded or read form file
  if(m->Rf!=NULL) { 
    if(fclose(m->Rf)) die("Error closing .il.R file");
    m->Rf=NULL;
  }
  if(m->Cf!=NULL) {
    if(fclose(m->Cf)) die("Error closing .il.C file");
    m->Cf=NULL;
  }
  free(m);
}


// decode a terminal representing a matrix entry
// the matrix value is multiplied by the corresponding X entry 
xmatval decode_entry(int p, rematrix *m, vector *x)
{
  size_t pcol = p % m->cols;
  size_t pval = p/m->cols;
  if(pval>=m->Mnum) die("Illegal value reference found in terminal");
  assert(pcol<x->size);
  return ((xmatval) x->v[pcol])*m->Mval[pval];
}  

vector *vector_create()
{
  vector *w = malloc(sizeof(vector));
  w->v = NULL;
  w->size=0;
  return w;
}

void vector_destroy(vector *v)
{
  if(v->v!=NULL) free(v->v);
  free(v);
}


// read a set of matval values from file f
// return number of items in *n and pointer to array with values
matval *read_vals(FILE *f, size_t *n)
{
  // get files size
  if(fseek(f,0,SEEK_END))
    die("Error in read_vals:fseek");
  long size = ftell(f);
  if(size<0)
    die("Error in read_vals:ftell");
  // get array size  
  *n = (int) (size/sizeof(matval));
  matval *a  = (matval *)malloc(*n * sizeof(matval));
  if(a==NULL)
    die("Error in read_vals:malloc");
  rewind(f);
  size_t e = fread(a,sizeof(matval),*n,f);
  if(e!= *n)
    die("Error in read_vals:fread");
  return a;
}



// compute the value associated to each non-terminal
// possibly overwriting the rules array
// Note: currently we are always using share=false
static void fill_NTval(rematrix *m, vector *x, bool share) 
{
  if(m->NTrules==NULL) die("Invalid call to fill_NTval");

  if(share) {
    // make sure the overwriting is possible 
    assert(sizeof(matval)<=2*sizeof(int));
    // share the same memory space 
    m->NTval = (matval *) m->NTrules;
  }
  else {
    if(m->NTval==NULL) {
      m->NTval = (matval *) malloc(m->NTnum*sizeof(matval));
      if(m->NTval==NULL) die("Cannot allocate NTval");
    }
  } 
  
  // compute values 
  int *pair = m->NTrules;
  for(int i=0; i<(ssize_t) m->NTnum;i++) {  // i is number of NT computed so far
    xmatval sum = 0;
    for(int j=0;j<2;j++) {
      int p = pair[j];
      if(p>=m->Alpha) { // non terminal
        p -= m->Alpha;
        if(p>=i) die("Fatal Error: Forward rule");
        sum += m->NTval[p];
        if(DEBUG) printf("%d-nt:%d  ",j,p);//!!!!!!!!!!!!
      }
      else { // terminal symbol
        if(p<m->rows) {
          if(DEBUG) printf("%d-sep: %d  ",j,p);//!!!!!!!!
          die("Unique row separator found in rule");
        }
        sum += decode_entry(p-m->rows,m,x);
        #ifndef INT_VALS 
        if(DEBUG) printf("%d-t: col:%d val:%f ",j,(p-m->rows)%m->cols,m->Mval[(p-m->rows)/m->cols]);//!!!!!!!111
        #else
        if(DEBUG) printf("%d-t: col:%d val:%d ",j,(p-m->rows)%m->cols,m->Mval[(p-m->rows)/m->cols]);//!!!!!!!111
        #endif
      }
    }
    pair += 2;   // advance to next rule
    m->NTval[i]=sum;
    #ifndef INT_VALS 
    if(DEBUG) printf("  NT[%d]: %f\n",i,sum); //!!!!!!!!!
    #else 
    if(DEBUG) printf("  NT[%d]: %d\n",i,sum); //!!!!!!!!!
    #endif
  }
  
  // make NTrules[] invalid
  if(share) m->NTrules = NULL;
  return;  
}


// write error message and exit
static void die(const char *s)
{
  perror(s);
  exit(1);
}    


