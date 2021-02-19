/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * ReMatrix
 * 
 * operations on repair compressed matrices
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
#include "rematrix.h"



int main (int argc, char **argv) { 
  FILE *f;
  int rows,cols;
  
  // ----------- check input
  fputs("==== Command line:\n",stderr);
  for(int i=0;i<argc;i++)
   fprintf(stderr," %s",argv[i]);
  fputs("\n",stderr);     
  if (argc != 6) { 
   fprintf (stderr,"Usage:\n\t %s <matrix> rows cols <invector> <outvector> \n",argv[0]);
   exit(1);
  }
  // ----------- read and check # rows and cols 
  rows  = atoi(argv[2]);
  if(rows<1) die("Invalid number of rows");
  cols  = atoi(argv[3]);
  if(cols<1) die("Invalid number of columns");
  // ------------ read input vector
  f = fopen(argv[4],"rb");
  if(f==NULL) die("Cannot open input vector file");
  vector x = vector_create();
  x.v = read_vals(f,&x.size);
  if(x.size!=cols) die("Input vector size should be equal to # of columns");
  fclose(f);
  // ------------ read matrix
  rematrix m = remat_create(rows,cols,argv[1]); 
  
  // compute product
  vector y = vector_create();
  remat_mult(&m,&x,&y);
  
  // --- open output vector file 
  f = fopen (argv[5],"w");
  if (f == NULL) die("Cannot open output vector file");
  size_t e = fwrite(y.v,sizeof(matval),y.size,f);
  if(e!=y.size) die("Cannot write to output file");
  if(fclose(f)!=0) die("Cannot close output file");
  
  // destroy 
  vector_destroy(&y);
  vector_destroy(&x);
  remat_destroy(&m);
  exit(0);
}

