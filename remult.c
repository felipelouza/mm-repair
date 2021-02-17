/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * ReMult
 * 
 * matrix multiplication using a repair compressed matrix
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef int matval;  // type representing a matrix entry   


// some orrible global variables

int Alpha;    // alphabet size of input matrix representation/smallest non terminal symbol 
int NTnum;    // number of non terminals 

int rows,cols;   // # rows and columns of input matrix 
matval *NTvalue; // values associated to non-terminals 
matval *Xval;    // input vector
matval *Yval;    // output vector 
matval *Mval;    // set of distinct nonzero matrix values





// write error message and exit
static void die(const char *s)
{
  perror(s);
  exit(1);
}    

// read a set of matval values from file f
// return number of items in *n and pointer to array with values
static matval *read_vals(FILE *f, int *n)
{
  // get files size
  if(fseek(f,0,SEEK_END))
    die("Error in read_vals:fseek");
  long size = ftell(f);
  if(size<0)
    die("Error in read_vals:ftell");
  // get array size  
  *n = (int) (size/sizeof(matval));
  matval *a  = malloc(*n * sizeof(matval));
  if(a==NULL)
    die("Error in read_vals:malloc");
  rewind(f);
  int e = fread(a,sizeof(matval),*n,f);
  if(e!= *n)
    die("Error in read_vals:fread");
  return a;
}



int main (int argc, char **argv) { 
     char fname[1024];
     FILE *Tf,*Rf,*Cf;
     int i,len,c,u;
     struct stat s;

     fputs("==== Command line:\n",stderr);
     for(int i=0;i<argc;i++)
       fprintf(stderr," %s",argv[i]);
     fputs("\n",stderr);     
     if (argc != 2) { 
       fprintf (stderr,"Usage: %s <filename>\n"
        "Decompresses <filename> from its .C and .R "
        "extensions\n"
        "This is a version for integer sequences\n",argv[0]);
        exit(1);
     }
     // open rules file
     strcpy(fname,argv[1]);
     strcat(fname,".R");
     if (stat (fname,&s) != 0) { 
       fprintf (stderr,"Error: cannot stat file %s\n",fname);
       exit(1);
     }
     len = s.st_size;
     Rf = fopen (fname,"r");
     if (Rf == NULL) { 
       fprintf (stderr,"Error: cannot open file %s for reading\n",fname);
       exit(1);
     }
     // read alphabet size for the original input string 
     if (fread(&Alpha,sizeof(int),1,Rf) != 1) { 
       fprintf (stderr,"Error: cannot read file %s\n",fname);
       exit(1);
     }
     NTnum = (len-sizeof(int))/(2*sizeof(int)); // number of rules. ie non terminal 
     NTval =  (matval *) malloc(NTnum*sizeof(matval));  // array of rules 

     fclose(Rf);

     strcpy(fname,argv[1]);
     strcat(fname,".C");
     if (stat (fname,&s) != 0)
  { fprintf (stderr,"Error: cannot stat file %s\n",fname);
    exit(1);
  }
     c = len = s.st_size/sizeof(int); // number of symbols in C string
     Cf = fopen (fname,"r");
     if (Cf == NULL)
  { fprintf (stderr,"Error: cannot open file %s for reading\n",fname);
    exit(1);
  }
     Tf = fopen (argv[1],"w");
     if (Tf == NULL)
  { fprintf (stderr,"Error: cannot open file %s for writing\n",argv[1]);
    exit(1);
  }
     u = 0; f = Tf; ff = argv[1];
     // read one symbol of C at a time and expand it 
     for (;len>0;len--)
  { if (fread(&i,sizeof(int),1,Cf) != 1)
       { fprintf (stderr,"Error: cannot read file %s\n",fname);
         exit(1);
       }
    u += expand(i,0);
  }
     fclose(Cf);
     if (fclose(Tf) != 0)
  { fprintf (stderr,"Error: cannot close file %s\n",argv[1]);
    exit(1);
  }
  // here n is the number of rules, n+alpha the effective alphabet in C 
  long est_size = (long) ( (2.0*n+(n+c)*(float)blog(n+alph-1))/8) + 1;  
  fprintf (stderr,"IDesPair succeeded\n");
  fprintf (stderr,"   Original ints: %i\n",u);
  fprintf (stderr,"   Size of the original input alphabet: %i\n",alph);
  fprintf (stderr,"   Number of rules: %i\n",n);
  fprintf (stderr,"   Compressed sequence length: %i\n",c);
  fprintf (stderr,"   Maximum rule depth: %i\n",maxdepth);
  fprintf (stderr,"   Estimated output size (bytes): %ld\n",est_size);
  fprintf (stderr,"   Compression ratio: %0.2f%%\n", (100.0*8*est_size)/(u*blog(alph-1)));
  // original estimate:   ((4.0*(n-alph)+((n-alph)+c)*(float)blog(n-1))/(u*blog(alph-1))*100.0));
  exit(0);
   }

