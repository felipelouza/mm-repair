# Matrix multiplication on RePair compressed matrices


## Prerequisites 

* [sdsl-lite](https://github.com/simongog/sdsl-lite) (`git clone https://github.com/simongog/sdsl-lite.git` + 
`cd sdsl-lite` + `./install.sh`)

* [psutil](https://pypi.org/project/psutil/) (`pip install psutil`)


## Installation 

Clone/download then `make`


## Sample computation

Given the [covtype](https://www.kaggle.com/giovannimanzini/some-machine-learning-matrices?select=covtype) matrix in csv format (581012 rows, 54 columns) we compress it with the command:
```bash
matrepair -r somedir/covtype 581012 54
```
that creates in the directory `somedir` the files `covtype.val`, `covtype.vc`, `covtype.vc.R`, `covtype.vc.C`, `covtype.vc.R.iv`, `covtype.vc.C.iv`, and `covtype.vc.C.ansf.1`, representing the output of different grammar compression algorithms (see [Available Matrix Compression Formats](Available-Matrix-Compression-Formats)). 

Next, we create a vector containing 54 entries equal to 1.0 and store it to the file `x54.dbl`:
```bash
makevec.py x54.dbl 54 1
```
Now we can compute the matrix vector products $`y=Mx`$ and $`z^T = y^T M`$ with the command:
```bash
re32mm  -y y.dbl -z z.dbl somedir/covtype 581012 54 x54.dbl
```
The output vector `y.dbl` has length 581012 and contains the sum of the entries of each row:
```bash
od -An -t f8 y.dbl | head
                    10300                    10077
                    13195                    13219
                     9963                     9709
                    10409                    10298
                    10456                    10378
                    10629                    13361
                    12989                    10640
                     9626                     9513
                    10310                     9479
                     9475                     9531
```
The output vector `z.dbl` has length 54 and contains the sum of the entries of each column of $`A^T A`$:
```bash
od -An -t f8 z.dbl | head
           14549643830975             757939056719
              65655800755            1372019643035
             227139887795           13332507438427
            1032604199658            1089965982517
             697072520994           11059033547178
               2526285260                225296661
               1924930316                178077606
                 12992451                 43226269
                 24825698                 78839086
                  7002929                 33807845
```

---

## Parallel computation

The command 
```bash
matrepair -r -b3 somedir/covtype 581012 54
```
splits the input matrix into three blocks of rows which are compressed separately. The command therefore creates the files `covtype.val`, `covtype.3.0.vc`, `covtype.3.0.vc.R`, `covtype.3.0.vc.C`, `covtype.3.0.vc.R.iv`, `covtype.3.0.vc.C.iv`, and `covtype.3.0.vc.C.ansf.1`, and similarly `covtype.3.1.vc` ...  `covtype.3.1.vc.C.ansf.1`, `covtype.3.2.vc` ... `covtype.3.2.vc.C.ansf.1`. 

Next, we can compute the matrix vector products $`y=Mx`$ and $`z^T = y^T M`$ in parallel using the above row blocks with the command:
```bash
re32mm -b3 -y y.dbl -z z.dbl covtype 581012 54 x54.dbl
```

---

## Available Matrix Compression Formats

### CSRV
Compressed Sparse Row/Value format. The encoding consists of the files with extensions `.val` and `.vc`. The `.val` file contains the *distinct* nonzero entries of the input matrix represented as 8-byte doubles. The `.vc` file contains for each nonzero element *a = A[i][j]* an encoding of the pair *(id,j)* where *j* is the column index and *id* is the index of the position in the `.val` file containing the actual value *a*. 


### Re32
The `.vc` file of the CSRV format is grammar compressed with RePair. All symbols are represented by 32 bit unsigned integers. The encoding consists of the files with extensions `.val`, `.vc.R`, and `vc.C`


### ReIV
The `.vc.C` and `vc.R` files of the Re32 format are represented as packed arrays with the minimum number of bits per entry using the `int_vector` class from SDSL-lite.
The encoding consists of the files with extensions `.val`, `.vc.R.iv`, and `vc.C.iv`


### ReANS
The `.vc.C` file of the re32 format is compressed using the ANS-fold-1 algorithm. The `.vc.R` file is represented as a packed array.
The encoding consists of the files with extensions `.val`, `.vc.R.iv`, and `vc.C.ansf.1`


### ReANS32
The `.vc.C` file of the re32 format is compressed using the ANS-fold-1 algorithm. The encoding consists of the files with extensions `.val`, `.vc.R`, and `vc.C.ansf.1`


---

## Tools

### matrepair
Tool to compute the CSRV representation of a matrix and to grammar-compress it. The input matrix is assumed to be in `csv` format unless its name ends with the `.dbl` extension in that case it is assumed to be in dense format with a 8-byte double per entry. With option `-r` the tool shows a nice report detailing running times and compression ratios.

### re32mm
Tool to compute a series of left and right matrix-vector multiplications reporting the overall running time and peak memory usage. Takes as input a compressed matrix in Re32 format (files `.val`, `.vc.R`, `.vc.C`) of size *RxC*, a vector *x* of size *C* stored in a binary file and an integer parameter *n* and computes *n* times the operation *y=Mx*, *z=y^t M*, *x = z/|z|*.

### csrvmm
Analogous to *re32mm* (uses the same code) except that the input matrix is in Compressed Sparse Row Value (CSRV) format (files `.val` and `.vc` see below) 

### reivmm, reansmm, reans32mm
Analogous to *re32mm* except that the input matrix is expected to be in the format ReIV, ReANS, and  ReANS32

---

## Bulk testing 

The tool *mmtest.py* can be used to test compression and (parallel) matrix-vector multiplication on a set of different matrices. The matrices, and their number of rows and columns, are specified inside *mmtest.py* in the global variables `Files` and `Sizes`. The first variable is a list of file names while the second is a dictionary providing the number of rows and columns for each file (extra entries in `Sizes` are ignored). The matrices tested with this script must be in *csv* format.

The command
```bash 
mmtest.py -d /data mc
```
converts the input matrices from the `/data` directory from the *csv* format to the dense uncompressed format and applies to the latter the compressors *gzip* and *xz* showing the absolute size and the percentage with respect to the dense uncompressed matrix. Used for generating Table 1 in the paper. 


The command
```bash 
mmtest.py -b 8 -d /data mz
```
computes the CSRV and grammar representations of the input matrices from the `/data` directory and show their size as percentage of the dense uncompressed matrices. Before computing the CSRV representation the input matrices are split into 8 row blocks. Used for generating Table 1 in the paper.


The command
```bash 
mmtest.py -b 8 -d /data -n num mm
```
executes *num* iterations of the matrix multiplication algorithms *csrvmm*, *re32mm*, *reivmm* and *reansmm* showing the average time per iteration and the peak memory usage. The command assumes that the input matrices have been already split into 8 row blocks and compressed as described above. Used for generating Table 2 in the paper.


---

## Column reordering

For the column reordering algorithms, take a look at the `reordering` subfolder.

---

## Internal tools 

### mat2csrv.py
Tool to compute the CSRV representation of a matrix. The input matrix is assumed to be in `csv` format unless its name ends with the `.dbl` extension in that case it is assumed to be in dense format with a 8-byte double per entry. Used by *matrepair*. Outputs the `.vc` and `.val` files.

### brepair/irepair0
Tool using the RePair algorithm to grammar-compress a sequence of integers; the integer 0 is never compressed (ie, used in the rhs of a rule). Used by *matrepair* to compress the `.vc` file producing the `.vc.R` (rules) and `.vc.C` (sequence) files.

### sdsl/encode.x
Tool to encode a sequence of 32-bit integers as a sdsl integer vector using the minimum number of bits per entry from (https://github.com/simongog/sdsl-lite)[sdsl-lite]. Used by *matrepair* to generate the `.iv` files.

### ans/encode.x
Tool to encode a sequence of 32-bit integers using the *ANSfold-1* encoder from (https://github.com/mpetri/ans-large-alphabet)[ans-large-alphabet]. Used by *matrepair* to generate the `.ansf.1` files.


### mat2bin.py
Tool to convert a matrix in csv format into binary float or double format (possibly removing some trailing or leading rows/columns). All entries are represented so the outfile has size `rows*cols*sizeof(double/float)`. With the option `--strip` the tool simply strips trailing or leading columns, or leading rows maintaining the csv format. Used by the *mmtest* tool.


### makevec.py
Tool to create a vector of a given length and write it to a file in binary format (default 8-byte doubles). The vector is specified giving a set of values which are repeated cyclically.


