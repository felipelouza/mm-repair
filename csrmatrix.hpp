/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * CsrMatrix
 * 
 * operations on Compressed Sparse Row Representation 
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
#include <errno.h>

//TODO: remove USE_ANSIV2 along the code
#ifdef USE_ANSIV2
  #include <algorithm>
  #include <cstdint>
  #define ANSf 1
  #if SPLIT
    #define CFILE_EXT_CSR ".B.vc.ansf.1"
  #else
    #define CFILE_EXT_CSR ".vc.ansf.1"
  #endif
#else
  #if SPLIT
    #define CFILE_EXT_CSR ".B.vc.ansf.1.dec"
  #else
    #define CFILE_EXT_CSR ".vc"
  #endif
#endif

#define CSR_BUF_LOG2 10                  // log of (size decompression buffer)  

#define VFILE_EXT ".val"

#ifdef WCODE
  #ifdef SPLIT
    #define CSR_WFILE_EXT ".B.wcode.ansf.1.dec"
    #define CSR_WFILE_EXT_ANS ".B.wcode.ansf.1"
  #else
    #define CSR_WFILE_EXT ".wcode"
    #define CSR_WFILE_EXT_ANS ".wcode.ansf.1"
  #endif
#endif 

#define CRS_BUF_MASK ((1<<CSR_BUF_LOG2)-1)     // mask to recognize beginning of buffer

// set to 1 to print a lot of debug information 
#define DEBUG 0


// the use of float's has not been fully tested 
#ifndef INT_VALS 
  #ifndef FLOAT_VALS
  typedef double matval;  // type representing a matrix/vector entry
  #else
  typedef float  matval;  // type representing a matrix/vector entry
  #endif     
  typedef double xmatval;   // type representing a matrix entry with larger precision   
#else
  typedef int matval;       // type representing a matrix entry   
  typedef int xmatval;      // type representing a matrix entry with larger precision   
#endif

typedef double  matval;    // type representing a matrix/vector entry
typedef double xmatval;   // type representing a matrix entry with larger precision   

// report error message and terminates
static void csr_quit(const char *msg, int line, const char *file);
//#define die(s) csr_quit((s),__LINE__,__FILE__)


// include definitions for vectors dense uncompressed vectors 
//#include "vector.h"


// matrix represented as a re-pair grammar
typedef struct {
  int rows,cols;  // # rows and columns of input matrix 
  int *CSRseq;      // array C of repair grammar
  size_t CSRlen;    // len of C array
  size_t Mnum;    // number of distinct non zero matrix values
  matval *Mval;   // set of distinct nonzero matrix values
  FILE *CSRf;       // C file 
  size_t Clen;    // len of C array
  int *Cseq;      // uncompressed array C from repair; here is only a buffer of size 1<<CSR_BUF_LOG2
#ifdef USE_ANSIV2
  uint8_t *Ccseq; // ans-compressed array C from repair
  size_t Cclen;   // length of ans-compressed array
#endif
#ifdef WCODE
  FILE *Wf;       
  int32_t *W;
  size_t Wsize;
#endif
} csr_rematrix;  



// main prototypes
csr_rematrix *csr_remat_create(int r, int c, char *basename, bool csr_read_vals);
void csr_remat_destroy(csr_rematrix *v, bool free_vals);
void csr_remat_mult(csr_rematrix *m, vector *x, vector *y);
matval *csr_read_vals(FILE *f, size_t* size);
xmatval csr_decode_mult_entry(int p, csr_rematrix *m, vector *x);
xmatval csr_decode_entry(int p, csr_rematrix *m, size_t *c);


csr_rematrix *csr_remat_create(int r, int c, char *basename,bool read_values)
{
  char fname[PATH_MAX];
  FILE *f; struct stat s;
  csr_rematrix *m= (csr_rematrix*) malloc(sizeof(csr_rematrix));
  if(m==NULL) die("Cannot allocate matrix");
  
  m->rows=r; m->cols=c;

  // ------------ read csr values
  if(strlen(basename)+10>PATH_MAX) die("Illegal base name");
  strcpy(fname,basename);
  strcat(fname,CFILE_EXT_CSR);
  if (stat (fname,&s) != 0) die("Cannot stat csr (" CFILE_EXT_CSR ") file");
  #ifdef USE_ANSIV2  
  {
    f = fopen (fname,"r");
    if (f == NULL) die("Cannot open csr (" CFILE_EXT_CSR ") file");
    /**/
    m->Cclen = s.st_size;
    m->Ccseq = new uint8_t[m->Cclen];
    if(fread(m->Ccseq,sizeof(uint8_t),m->Cclen,f)!=m->Cclen)
     die("Cannot read " CFILE_EXT_CSR " file");    
    if(fclose(f)) 
      die("Error closing C (" CFILE_EXT_CSR ") file");
    // extract decompressed length and remove it from compressed data 
    m->Clen = *( (size_t *) m->Ccseq); // size of the decompressed C sequence 
    m->Ccseq += sizeof(size_t);
    m->Cclen -= sizeof(size_t);  
    // allocate decompressed buffer
    m->Cseq = (int *) malloc((1<<CSR_BUF_LOG2)*sizeof(int));
    if(m->Cseq==NULL) die("Cannot allocate buffer for ANS decompression");
    /*
    size_t fsize = s.st_size;
    uint8_t *csr_u8 = new uint8_t[fsize];
    if(fread(csr_u8,sizeof(uint8_t),fsize,f)!=fsize) die("Cannot read csr " CFILE_EXT_CSR " file");    
    if(fclose(f)) die("Error closing csr (" CFILE_EXT_CSR ") file");
    // size of compressed data
    size_t cSrcSize = fsize-sizeof(size_t);
    // retrieve decompressed file size
    size_t original_size = *((size_t *) csr_u8);
    auto ans_dec = ANSf_decoder<1>();
    ans_dec.init(csr_u8+sizeof(size_t), cSrcSize, original_size);
    m->CSRlen = original_size;
    m->CSRseq = (int *) malloc(m->CSRlen*sizeof(int));
    if(m->CSRseq == NULL)  die("Cannot allocate csr array");
    size_t decoded = 0;
    while(decoded < m->CSRlen) {
      size_t d = ans_dec.decode((uint32_t*)(m->CSRseq + decoded), std::min((size_t)(1<<CSR_BUF_LOG2), m->CSRlen - decoded));
      if(d == 0) die("Error decoding WCODE file");
      decoded += d;
    } 
    delete[] csr_u8;
    */  
  }
  #else
    m->CSRf = fopen (fname,"r");
    if (m->CSRf == NULL)      die("Cannot open csr (" CFILE_EXT_CSR ") file");
    m->CSRlen = (s.st_size)/sizeof(int);
   
/*
    fprintf(stderr, "fname = %s\t%d\n", fname, m->CSRlen);
    m->CSRseq = (int *) malloc(m->CSRlen*sizeof(int));
    if(fread(m->CSRseq,sizeof(int),m->CSRlen,m->CSRf)!=m->CSRlen)
     die("Cannot read .vc file");
*/
    m->Cseq = (int *) malloc((1<<CSR_BUF_LOG2)*sizeof(int));
    if(m->Cseq==NULL) die("Cannot allocate buffer for decompression");
  
/*
    #if OLE
    uint32_t acc=0;
    for(size_t j=0; j < m->CSRlen; j++) {
      int i = m->CSRseq[j];
      if(i==0) acc=0;
      else m->CSRseq[j] += acc;
      acc = m->CSRseq[j];
    }
    #endif
*/

  #endif
  
  // ------------ read matrix values 
  if(read_values) {
    strcpy(fname,basename);
    strcat(fname,VFILE_EXT);
    f = fopen(fname,"rb");
    if(f==NULL) die("Cannot open matrix values (" VFILE_EXT ") file");
    m->Mval = csr_read_vals(f,&m->Mnum);
    if(fclose(f)!=0) die("Error closing values (" VFILE_EXT ") file");
  }
  else {
    m->Mval=NULL; m->Mnum=0;
  }

  // --- open WCODE file
  #ifdef WCODE
  strcpy(fname,basename);
    #ifdef USE_ANSIV2
    {
      strcat(fname,CSR_WFILE_EXT_ANS);
      if (stat (fname,&s) != 0) die("Cannot stat WCODE (" CSR_WFILE_EXT_ANS ") file");
      f = fopen (fname,"r");
      if (f == NULL) die("Cannot open WCODE (" CSR_WFILE_EXT_ANS ") file");
      size_t fsize = s.st_size;
      uint8_t *in_u8 = new uint8_t[fsize];
      if(fread(in_u8,sizeof(uint8_t),fsize,f)!=fsize) die("Cannot read WCODE " CSR_WFILE_EXT_ANS " file");    
      if(fclose(f)) die("Error closing WCODE (" CSR_WFILE_EXT_ANS") file");
      // size of compressed data
      size_t cSrcSize = fsize-sizeof(size_t);
      // retrieve decompressed file size
      size_t original_size = *((size_t *) in_u8);
      auto ans_dec = ANSf_decoder<1>();
      ans_dec.init(in_u8+sizeof(size_t), cSrcSize, original_size);
      m->Wsize = original_size;
      m->W = (int32_t*) malloc(m->Wsize*sizeof(int32_t));
      if(m->W == NULL)  die("Cannot allocate WCODE array");
      size_t decoded = 0;
      while(decoded < m->Wsize) {
        size_t d = ans_dec.decode((uint32_t*)(m->W + decoded), std::min((size_t)(1<<CSR_BUF_LOG2), m->Wsize - decoded));
        if(d == 0) die("Error decoding WCODE file");
        decoded += d;
      }   
      delete[] in_u8;
    }
    #else
      strcat(fname,CSR_WFILE_EXT);
      f = fopen(fname, "rb"); 
      if(f == NULL) die("Cannot open WCODE file");
      if(fseek(f, 0, SEEK_END)) die("Error seeking WCODE file");
      long size = ftell(f);
      if(size < 0) die("Error reading WCODE file size");
      rewind(f);
      m->Wsize = size / sizeof(int32_t);
      m->W = (int32_t *) malloc(m->Wsize * sizeof(int32_t));
      if(m->W == NULL)  die("Cannot allocate WCODE array");
      if(fread(m->W, sizeof(int32_t), m->Wsize, f) != m->Wsize)
        die("Cannot read WCODE file");
      fclose(f); 
    #endif
    int i=0;
    for(i=1;i<m->Wsize; i++)m->W[i]+=m->W[i-1];
  
    //TODO 
    ///**/
    //for(size_t j=0; j < m->CSRlen; j++) {
    //  m->CSRseq[j] = m->W[m->CSRseq[j]];
    //}
    //free(m->W);
    /**/
  #endif



  return m;
}

// right multiplication 
void csr_remat_mult(csr_rematrix *m, vector *x, vector *y)
{
  if(m->cols!=x->size) die("Dimension mismatch (csr_remat_mult x)");   
  if(m->rows!=y->size) die("Dimension mismatch (csr_remat_mult y)");   

  // --- compute output 
  #ifdef USE_ANSIV2
    // create and initialize decoder
    auto ans_dec = ANSf_decoder<ANSf>();
    ans_dec.init(m->Ccseq, m->Cclen, m->Clen);
  #else
      rewind(m->CSRf);
  #endif
  int ycur = 0;
  xmatval sum=0;
  #ifdef OLE
    uint32_t acc=0;
  #endif
  #ifdef USE_ANSIV2
  for(size_t j=0; j < m->Clen; j++) {
  #else
  for(size_t j=0; j < m->CSRlen; j++) {
  #endif
    if((j & CRS_BUF_MASK) ==0) {
      // fill the buffer 
      #ifdef USE_ANSIV2
        size_t to_read = std::min((size_t)(1<<CSR_BUF_LOG2), m->Clen -j);
        size_t d = ans_dec.decode((uint32_t *)m->Cseq,to_read);
        if(d==0) die("Illegal decode call");
      #else
        size_t to_read = std::min((size_t)(1<<CSR_BUF_LOG2), m->CSRlen -j);
        if(fread(m->Cseq,sizeof(int),to_read,m->CSRf)!=to_read){
          die("Cannot read .vc file %d");
        }
      #endif
      #ifdef OLE
      size_t j2 = 0;
      while(j2 < to_read){
        if(m->Cseq[j2]==0) acc=0;
        else m->Cseq[j2] += acc;
        acc = m->Cseq[j2];
        j2++;
      }
      #endif
    }
//    #endif
    #ifdef USE_ANSIV2
      int i = m->Cseq[j&CRS_BUF_MASK]; // read a single int from buffer    
    #else
      int i = m->Cseq[j&CRS_BUF_MASK]; // read a single int from buffer    
      //int i = m->CSRseq[j];
    #endif
    if(i>0) {// symbol representing a matrix entry
     sum += csr_decode_mult_entry(i,m,x);
    }
    else { // i==0 row completed
     y->v[ycur] += (matval) sum;
     sum = 0;
     #ifdef USE_ANSIV2
       if(++ycur==y->size) assert(j+1==m->Clen);
     #else
       if(++ycur==y->size) assert(j+1==m->CSRlen);
     #endif
    }
  }
  assert(ycur==y->size);
  assert(sum==0);
}


// left multiply the (rows x cols) matrix m by the
// vector y^T of size (1 x rows), obtaining x^T of size (1 x cols)
void csr_remat_left_mult(vector *y, csr_rematrix *m, vector *x)
{
  // make sure dimensions agree
  if(m->rows!=y->size) die("Dimension mismatch (csr_remat_left_mult y)");   
  if(m->cols!=x->size) die("Dimension mismatch (csr_remat_left_mult x)");   
  // clean x
  //for(size_t i=0;i<x->size;i++) x->v[i]=0;

  // variables used by csr_decode_entry 
  #ifdef USE_ANSIV2
    // create and initialize decoder
    auto ans_dec = ANSf_decoder<ANSf>();
    ans_dec.init(m->Ccseq, m->Cclen, m->Clen);
  #else
      rewind(m->CSRf);
  #endif
  xmatval a; size_t col;   
  // propagate y-values to symbols in C
  int ycur=0; // ycur is the current row index 
  #ifdef OLE
    uint32_t acc=0;
  #endif
  #if USE_ANSIV2
  for(size_t j=0; j<m->Clen;j++) {  
  #else
  for(size_t j=0; j<m->CSRlen;j++) {  
  #endif
    if((j & CRS_BUF_MASK) ==0) {
      #if USE_ANSIV2
        size_t to_read = std::min((size_t)(1<<CSR_BUF_LOG2), m->Clen -j);
        size_t d = ans_dec.decode((uint32_t *)m->Cseq,to_read);
        if(d==0) die("Illegal decode call");
      #else
        size_t to_read = std::min((size_t)(1<<CSR_BUF_LOG2), m->CSRlen -j);
        if(fread(m->Cseq,sizeof(int),to_read,m->CSRf)!=to_read){
          die("Cannot read .vc file");
        }
      #endif
      #ifdef OLE
      size_t j2 = 0;
      while(j2 < to_read){
        if(m->Cseq[j2]==0) acc=0;
        else m->Cseq[j2] += acc;
        acc = m->Cseq[j2];
        j2++;
      }
      #endif
    }
//    #endif
    #if USE_ANSIV2
      int i = m->Cseq[j&CRS_BUF_MASK];    // read a single int from buffer
    #else
      int i = m->Cseq[j&CRS_BUF_MASK]; // read a single int from buffer    
      //int i = m->CSRseq[j];
    #endif
    if(i>0) {         // symbol representing a matrix entry
      a = csr_decode_entry(i,m,&col);
      assert(col<x->size);
      x->v[col] += a * y->v[ycur];
    }
    else { // i==0 row completed
      #ifdef USE_ANSIV2
        if(++ycur==y->size) assert(j+1==m->Clen);
      #else
        if(++ycur==y->size) assert(j+1==m->CSRlen);
      #endif
    }
  }
  assert(ycur==y->size);
}


void csr_remat_destroy(csr_rematrix *m, bool free_vals)
{  
  if(free_vals) free(m->Mval);
  // these are usually accessed sequentially, so one could keep them on file
  if(m->CSRseq)    {free(m->CSRseq); m->CSRseq=NULL;}
  // the file .vc were left open in case data have to be reloaded or read form file
  if(m->CSRf!=NULL) {
    if(fclose(m->CSRf)) die("Error closing .vc file");
    m->CSRf=NULL;
  }
  #ifdef USE_ANSIV2
    if(m->Ccseq!=NULL) {
      delete[] (m->Ccseq - sizeof(size_t)); // see initialization of m->Ccseq
      m->Ccseq = NULL;
    } 
  #endif
  if(m->Cseq)    {free(m->Cseq); m->Cseq=NULL;}
  #ifdef WCODE
    free(m->W);
  #endif
  free(m);
}


// get value and column from terminal representing a matrix entry
xmatval csr_decode_entry(int p, csr_rematrix *m, size_t *c)
{
  #ifdef WCODE
    p = m->W[p];
  #endif
  p = p-1;
  *c = p % m->cols;
  size_t pval = p/m->cols;
  if(pval>=m->Mnum) die("Illegal value reference found in terminal symbol (csr_decode_entry)");
  return m->Mval[pval];  
}


// decodes a terminal representing a matrix entry
// does not return the column index, instead the matrix
// value is multiplied by the corresponding X entry 
xmatval csr_decode_mult_entry(int p, csr_rematrix *m, vector *x)
{
  #ifdef WCODE
    p = m->W[p];
  #endif
  p = p-1;
  size_t pcol = p % m->cols;
  size_t pval = p/m->cols;
  if(pval>=m->Mnum) die("Illegal value reference found in terminal symbol (csr_decode_mult_entry)");
  assert(pcol<x->size);
  return ((xmatval) x->v[pcol])*m->Mval[pval];
}  

// read a set of matval values from file f
// return number of items in *n and pointer to array with values
matval *csr_read_vals(FILE *f, size_t *n)
{
  // get files size
  if(fseek(f,0,SEEK_END))
    die("Error in csr_read_vals:fseek");
  long size = ftell(f);
  if(size<0)
    die("Error in csr_read_vals:ftell");
  // get array size  
  *n = (int) (size/sizeof(matval));
  matval *a  = (matval *)malloc(*n * sizeof(matval));
  if(a==NULL)
    die("Error in csr_read_vals:malloc");
  rewind(f);
  size_t e = fread(a,sizeof(matval),*n,f);
  if(e!= *n)
    die("Error in csr_read_vals:fread");
  return a;
}

// write error message + extra info and and exit
static void csr_quit(const char *msg, int line, const char *file) {
  if(errno==0)  fprintf(stderr,"== %d == %s\n",getpid(), msg);
  else fprintf(stderr,"== %d == %s: %s\n",getpid(), msg,
               strerror(errno));
  fprintf(stderr,"== %d == Line: %d, File: %s\n",getpid(),line,file);
  exit(1);
}

