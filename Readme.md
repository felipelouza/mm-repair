# Better Matrix–Vector Multiplication via Hybrid Matrix Compression

`mm-RePair-H` is a lossless matrix compression framework supporting matrix–vector multiplication directly over compressed data. It extends the original [**mm-RePair**](https://gitlab.com/manzai/mm-repair/) by combining **grammar compression** with **entropy coding**, improving compression ratio and reducing peak memory consumption while preserving efficient matrix–vector multiplication.

The main compression program is `matrepair`. The hybrid compression scheme can be enabled with the `--hybrid` option. The `matrepair-h` executable is a convenience wrapper that invokes `matrepair --hybrid`.

## Prerequisites

- Python 3.8 or later
- [sdsl-lite](https://github.com/simongog/sdsl-lite/)
- [psutil](https://pypi.org/project/psutil/)

To install `sdsl-lite`:

```bash
git clone https://github.com/simongog/sdsl-lite.git
cd sdsl-lite
./install.sh
```

To install `psutil`:

```bash
pip install psutil
```

## Installation

```bash
git clone https://github.com/felipelouza/mm-repair.git
cd mm-repair
make
```

## Compression

To use the hybrid scheme, add the `--hybrid` option to matrepair:

```bash
./matrepair --hybrid input.csv 8 6
```

The `matrepair-h` executable provides a shorthand for the same command.

The hybrid scheme combines three techniques:

1. **Alphabet mapping:** the CSRV alphabet is remapped to a compact range of consecutive integers.
2. **Hybrid partitioning:** the CSRV sequence is partitioned into two components. One component is grammar-compressed with RePair, while the other is encoded directly with ANS-fold.
3. **Ordered-list encoding:** entries within each row are reordered and delta-encoded before entropy coding.

## Running example 

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

### 📁 Data compression

Compress the matrix using:

```bash
./matrepair --hybrid input.csv 8 6
```

This produces files:

```text
input.csv.val
input.csv.vc.wcode
input.csv.A.vc.C.ansf.1
input.csv.A.vc.R.iv
input.csv.B.vc.ansf.1
```

These files store the compressed representation of the two components produced by the hybrid compression scheme: **Part A**, which is grammar-compressed with RePair, and **Part B**, which is compressed with ANS-fold.

#### 🔨 Compression details

The file `input.csv.val` stores the distinct nonzero values appearing in `input.csv`:

```text
V = [5.3, 8.1, 6.0, 2.7]
```

The CSV matrix is converted into its CSRV representation (`input.csv.vc`). During this step, the CSRV alphabet is remapped to a compact range of consecutive integers. The corresponding mapping is stored in:

```text
input.csv.vc.wcode
```

The remapped CSRV sequence is then partitioned into two components:

- `input.csv.A.vc`, containing the symbols selected for grammar compression with RePair;
- `input.csv.B.vc`, containing the remaining symbols, which are encoded directly with ANS-fold.

RePair is applied to `input.csv.A.vc`:

```text
==== RePair compression
Command: /home/louza/mm-repair/brepair/irepair0 input.csv.A.vc 17733
```

This produces the grammar (`input.csv.A.vc.R`) and the compressed sequence (`input.csv.A.vc.C`).

The grammar is then encoded as a packed integer vector using SDSL:

```text
==== Integer vector compression
Command: /home/louza/mm-repair/sdsl/encode.x input.csv.A.vc.R
```

producing:

```text
input.csv.A.vc.R.iv
```

Finally, the RePair final sequence and the second CSRV component are encoded using the ordered-list encoding (OLE) and ANS-fold. The resulting files are:

```text
input.csv.A.vc.C.ansf.1
input.csv.B.vc.ansf.1
```

### 🍫 Matrix–vector multiplication

Next, create a vector containing six entries equal to `1.0`:

```bash
./makevec.py x6.dbl 6 1
```

The output vector `x6.dbl` has length 6 and contains only ones:

```bash
od -An -v -t f8 x6.dbl
```

```text
                        1                        1
                        1                        1
                        1                        1
```

Finally, we compute the matrix-vector products `y = Ax` and `z^T = y^T A` directly over the compressed representation with:

```bash
./remm-h -y y.dbl -z z.dbl input.csv 8 6 x6.dbl
Elapsed time: 0 secs
```

The vector `y.dbl` has length 8 and contains the sum of the entries in each row:

```bash
od -An -t f8 y.dbl
```

```text
                   19.4                    22.0
                   24.7                    14.7
                   11.3                    22.7
                   16.8                    16.7
```

The vector `z.dbl` has length 6 and contains the entries of:

```text
A^T y
```

```bash
od -An -t f8 z.dbl
```

```text
                   546.73                   61.29
                   826.41                   250.83
                   537.51                   667.88
```

---

## Bulk testing

The `mmtest-h.py` script can be used to evaluate compression and matrix–vector multiplication on a collection of matrices.

The input matrices and their dimensions are specified in `mmtest.py` using the global variables `Files` and `Sizes`. `Files` is a list of input file names, while `Sizes` is a dictionary containing the number of rows and columns for each file. Entries in `Sizes` that do not correspond to a file in `Files` are ignored.

The default values of `Files` and `Sizes` can be overridden using the `--files` and `--sizes` options.

### Compression

The command

```bash
./mmtest-h.py mz -b 2 -d /data
```

computes the CSRV and compressed representations of the input matrices in `/data` and reports their sizes as percentages of the corresponding dense, uncompressed matrices.

The `-b 2` option partitions each input matrix into 2 row blocks before computing its CSRV representation.

### Matrix–vector multiplication

The command

```bash
./mmtest-h.py mm -b 2 -d /data -n num
```

executes `num` iterations of the matrix–vector multiplication algorithms `csrvmm`, `re32mm`, `reivmm`, and `reansmm`, reporting the average execution time per iteration and the peak memory usage.

The command assumes that the input matrices have already been partitioned into 2 row blocks and compressed as described above.

---

## Authors

* [Felipe Louza](https://github.com/felipelouza)
* [Giovanni Manzini](https://gitlab.com/manzai)
* [Guilherme Telles](https://github.com/gptelles)


