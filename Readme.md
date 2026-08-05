# Better Matrix–Vector Multiplication via Hybrid Matrix Compression

`mm-RePair-H` is a lossless matrix compression framework supporting matrix–vector multiplication directly over compressed data. It extends the original [**mm-RePair**](https://gitlab.com/manzai/mm-repair/) by combining **grammar compression** with **entropy coding**, improving compression ratio and reducing peak memory consumption while preserving efficient matrix–vector multiplication.

## Prerequisites 

* Python version 3.8 or later
* [sdsl-lite](https://github.com/simongog/sdsl-lite) (`git clone https://github.com/simongog/sdsl-lite.git` + 
`cd sdsl-lite` + `./install.sh`)
* [psutil](https://pypi.org/project/psutil/) (`pip install psutil`)


## Installation 

```sh
git clone https://github.com/felipelouza/mm-repair.git
cd mm-repair
make 
```

## Sample computation

Consider the following [input.csv](https://github.com/felipelouza/mm-repair/blob/master/input.csv) matrix (8 rows × 6 columns):

```text
5.3, 0.0, 8.1, 0.0, 6.0, 0.0
2.7, 0.0, 6.0, 2.7, 5.3, 5.3
5.3, 0.0, 8.1, 0.0, 6.0, 5.3
6.0, 0.0, 0.0, 2.7, 0.0, 6.0
0.0, 0.0, 0.0, 0.0, 6.0, 5.3
5.3, 2.7, 6.0, 2.7, 0.0, 6.0
0.0, 0.0, 6.0, 2.7, 0.0, 8.1
2.7, 0.0, 6.0, 2.7, 5.3, 0.0
```

### :file_folder: Data Compression

We compress it with the command:

```bash
$ ./matrepair-h -r input.csv 8 6
```

This creates the files `input.csv.val`, `input.csv.vc.wcode`, `input.csv.A.vc.C.ansf.1`, `input.csv.A.vc.R.iv` and `input.csv.B.vc.ansf.1`. 

These files correspond to the compressed representations of the two components produced by the hybrid compression scheme: Part A, compressed with RePair, and Part B, compressed with ANS-fold.

#### :hammer::hammer::hammer: Details

The file `input.csv.val` stores the distinct nonzero values appearing in `input.csv`:

```text
V = [5.3, 8.1, 6.0, 2.7]
```

The CSV matrix is converted into its CSRV representation (`input.csv.vc`). During this step, the CSRV alphabet is remapped to a compact range of consecutive integers, and the mapping is stored in `input.csv.vc.wcode`.

The remapped CSRV sequence is then partitioned into two components:

* `input.csv.A.vc`, containing the symbols that are compressed with RePair;
* `input.csv.B.vc`, containing the remaining symbols, which are compressed directly with ANS-fold.

RePair is applied to `input.csv.A.vc`:

```bash
==== RePair compression
Command: /home/louza/mm-repair/brepair/irepair0 input.csv.A.vc 17733
```

This step produces the grammar (`input.csv.A.vc.R`) and the final sequence (`input.csv.A.vc.C`).

The grammar is then encoded into as a packed integer vector using SDSL, producing `input.csv.A.vc.R.iv`. 

```bash
==== Integer vector compression
Command: /home/louza/mm-repair/sdsl/encode.x input.csv.A.vc.R
```

At the end, the RePair sequence (`input.csv.A.vc.C`) and the second CSRV component (`input.csv.B.vc`) are compressed using OLE and ANS-fold, producing the files:

```text
input.csv.A.vc.C.ansf.1
input.csv.B.vc.ansf.1
```


### :chocolate_bar: Matrix–Vector Multiplication 

Next, we create a vector containing six entries equal to `1.0`:

```bash
$ ./makevec.py x6.dbl 6 1
```

The output vector `x6.dbl` has length 6 and contains only ones:

```bash
$ od -An -v -t f8 x6.dbl 
                        1                        1
                        1                        1
                        1                        1
```


Finally, we compute the matrix-vector products (y = Ax) and (z^T = y^T A) with

```bash
$ ./remm-h -y y.dbl -z z.dbl input.csv 8 6 x6.dbl
Elapsed time: 0 secs
```

The output vector `y.dbl` has length 8 and contains the sum of the entries of each row:

```bash
$ od -An -t f8 y.dbl
                   19.4                    22.0
                   24.7                    14.7
                   11.3                    22.7
                   16.8                    16.7
```

The output vector `z.dbl` has length 6 and contains the entries of the vector (A^T y):

```bash
$ od -An -t f8 z.dbl
                   546.73                   61.29
                   826.41                   250.83
                   537.51                   667.88

```
---

## Bulk testing 

The tool *mmtest-h.py* can be used to test compression and (parallel) matrix-vector multiplication on a set of different matrices. The matrices, and their number of rows and columns, are specified inside *mmtest.py* in the global variables `Files` and `Sizes`. The first variable is a list of file names while the second is a dictionary providing the number of rows and columns for each file (extra entries in `Sizes` are ignored). The default content of the variables `Files` and `Sizes` can be overriden by the options `--files` and `-sizes`.

The command
```bash 
mmtest-h.py mz -b 8 -d /data
```
computes the CSRV and grammar representations of the input matrices from the `/data` directory and show their size as percentage of the dense uncompressed matrices. Before computing the CSRV representation the input matrices are split into 8 row blocks.


The command
```bash 
mmtest-h.py mm -b 8 -d /data -n num
```
executes *num* iterations of the matrix multiplication algorithms *csrvmm*, *re32mm*, *reivmm* and *reansmm* showing the average time per iteration and the peak memory usage. The command assumes that the input matrices have been already split into 8 row blocks and compressed as described above. 

---

## Internal tools 


### csvmat2csrv
Tool to compute the CSRV representation of a matrix. The input matrix is assumed to consists of `float64` (doubles) stored in `csv` format (one row per line). 
Used by *materepair-h*. Outputs the `.vc` and `.val` files. 


### bin2csrv, bin2csrvf, bin2csrvi
Tools to compute the CSRV representation of a matrix stored in binary form. 
The three versions assume that the matrix entries are stored respectivley as float64 (double), float32, int32 and write such entries in the '[if]val' file in the same format. 
Used by *materepair-h*. Outputs the `.vc` and `.[if]val` files. 


### brepair/irepair0
Tool using the RePair algorithm to grammar-compress a sequence of integers; the integer 0 is never compressed (ie, used in the rhs of a rule). Used by *materepair-h* to compress the `.vc` file producing the `.vc.R` (rules) and `.vc.C` (sequence) files.

### sdsl/encode.x
Tool to encode a sequence of 32-bit integers as a sdsl integer vector using the minimum number of bits per entry from (https://github.com/simongog/sdsl-lite)[sdsl-lite]. Used by *materepair-h* to generate the `.iv` files.

### ans/encode.x
Tool to encode a sequence of 32-bit integers using the *ANSfold-1* encoder from (https://github.com/mpetri/ans-large-alphabet)[ans-large-alphabet]. Used by *materepair-h* to generate the `.ansf.1` files.

### ole/encode.x
Tool to reorder elements within a row (separated by 0's) and delta-encode (gap) the values. Used by *materepair-h* to generate the `.A.vc.C.ansf.1` file.


### others/csvmat2bin.py
Tool to convert a matrix in csv format into binary int32/float32/float64 format (possibly removing some trailing or leading rows/columns). All matrix entries are represented so the outfile has size `rows*cols*sizeof(entry)`. Note that when using the int32 or float32 output formats some information will be lost if the input values are not of the right type.


### others/mat2csrv.py
Tool to compute the CSRV representation of a matrix. The input matrix is assumed to be in `csv` format unless its name ends with the `.dbl` extension in that case it is assumed to be in dense format with a 8 byte double per entry. Outputs the `.vc` and `.val` files. Superseeded by `csvmat2csrv` and  `bin2csrv[if]`.
