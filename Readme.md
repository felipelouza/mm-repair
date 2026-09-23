# Better Matrix–Vector Multiplication via Hybrid Matrix Compression

`mm-RePair-H` is a lossless matrix compression framework that supports matrix–vector
multiplication directly over the compressed data, with no need to decompress first.

It extends [**mm-RePair**](https://gitlab.com/manzai/mm-repair/) by combining **grammar
compression** with **entropy coding**, which improves the compression ratio and lowers peak
memory usage while keeping matrix–vector multiplication efficient.

The main compression program is `matrepair`. The hybrid scheme is enabled with `--hybrid`.

## How the hybrid scheme works

1. **Hybrid partitioning** — the CSRV sequence is split into two components. Part A is
   grammar-compressed with RePair; part B is encoded directly with ANS-fold.
2. **Alphabet mapping** — the CSRV alphabet is remapped to a compact range of consecutive
   integers.
3. **Ordered-list encoding (OLE)** — entries within each row are reordered and delta-encoded
   before entropy coding.

## Prerequisites

- A C++ compiler with C++11 support, plus `make`
- Python 3.8 or later
- [sdsl-lite](https://github.com/simongog/sdsl-lite/)
- [psutil](https://pypi.org/project/psutil/)

Install `sdsl-lite`:

```bash
git clone https://github.com/simongog/sdsl-lite.git
cd sdsl-lite
./install.sh
```

Install `psutil`:

```bash
pip install psutil
```

## Installation

```bash
git clone https://github.com/felipelouza/mm-repair.git
cd mm-repair
make
```

## Usage

```bash
# Compression
./matrepair [--hybrid] [-r] [-b <blocks>] <matrix.csv> <rows> <cols>

# Build a dense vector of a constant value
./makevec.py <output.dbl> <length> <value>

# Matrix–vector multiplication over the compressed data
./remm-h [-y <y.dbl>] [-z <z.dbl>] [-b <blocks>] [-n <iters>] [-v] <basename> <rows> <cols> <x.dbl>
```

The input matrix is given as a CSV file of floating-point values; `rows` and `cols` are its
dimensions. The `-r` option prints a report of running times and compression ratios, and `-b`
splits the matrix into row blocks that are compressed separately.

For `remm-h`, the first positional argument is the base name of the compressed files, not the
matrix: the multiplication reads only `basename.val`, `basename.wcode`, `basename.A.vc.*` and
`basename.B.vc.*`. Since `matrepair` names its output after the input file, this is the same
string you passed to it. Use the same `-b` value used at compression time; with `-n`, the
tool repeats the products `n` times, renormalising `x = z/‖z‖` between iterations.

## Running example

The repository ships with
[`input.csv`](https://github.com/felipelouza/mm-repair/blob/master/input.csv), an 8 × 6
matrix:

```text
5.3, 0.0, 8.1, 8.1, 6.0, 5.3
5.3, 0.0, 0.0, 2.7, 0.0, 6.0
5.3, 0.0, 8.1, 0.0, 6.0, 5.3
6.0, 0.0, 0.0, 2.7, 0.0, 6.0
0.0, 0.0, 0.0, 0.0, 6.0, 5.3
2.7, 0.0, 6.0, 2.7, 5.3, 0.0
0.0, 0.0, 6.0, 8.1, 0.0, 8.1
2.7, 0.0, 6.0, 2.7, 5.3, 0.0
```

### Compression

```bash
./matrepair --hybrid input.csv 8 6
```

This produces the compressed representation:

| File | Contents |
| --- | --- |
| `input.csv.val` | the distinct nonzero values of the matrix |
| `input.csv.wcode` | the CSRV alphabet mapping |
| `input.csv.A.vc.R.iv` | the RePair grammar of part A, as a packed integer vector |
| `input.csv.A.vc.C.ansf.1` | the RePair final sequence of part A, OLE + ANS-fold encoded |
| `input.csv.B.vc.ansf.1` | part B, OLE + ANS-fold encoded |

#### What happens along the way

`input.csv.val` stores the distinct nonzero values appearing in the matrix:

```text
V = [5.3, 8.1, 6.0, 2.7]
```

The matrix is converted into its CSRV representation (`input.csv.vc`), and the CSRV alphabet
is remapped to a compact range of consecutive integers. The mapping is written to
`input.csv.wcode`.

The remapped sequence is then partitioned into two components:

- `input.csv.A.vc` — the symbols selected for grammar compression with RePair;
- `input.csv.B.vc` — the remaining symbols, encoded directly with ANS-fold.

RePair is applied to part A:

```text
==== RePair compression
Command: ./mm-repair/brepair/irepair0 input.csv.A.vc 17733
```

producing the grammar (`input.csv.A.vc.R`) and the compressed sequence
(`input.csv.A.vc.C`). The grammar is encoded as a packed integer vector using SDSL:

```text
==== Integer vector compression
Command: ./mm-repair/sdsl/encode.x input.csv.A.vc.R
```

which yields `input.csv.A.vc.R.iv`. Finally, the RePair final sequence and part B are encoded
with the ordered-list encoding and ANS-fold, giving `input.csv.A.vc.C.ansf.1` and
`input.csv.B.vc.ansf.1`.

### Matrix–vector multiplication

Create a vector of six entries equal to `1.0`:

```bash
./makevec.py x6.dbl 6 1
od -An -v -t f8 x6.dbl
```

```text
                        1                        1
                        1                        1
                        1                        1
```

Then compute `y = Ax` and `z = Aᵀy` (equivalently `zᵀ = yᵀA`) directly over the compressed
representation. Here `input.csv` is the base name of the compressed files written above, not
the matrix:

```bash
./remm-h -y y.dbl -z z.dbl input.csv 8 6 x6.dbl
```

```text
Elapsed time: 0 secs
```

Since `x` is all ones, `y` has length 8 (one entry per row) and holds the sum of the entries
in each row:

```bash
od -An -t f8 y.dbl
```

```text
                     32.8                     14.0
                     24.7                     14.7
                     11.3                     16.7
                     22.2                     16.7
```

`z` has length 6 (one entry per column) and holds `Aᵀy`:

```bash
od -An -t f8 z.dbl
```

```text
                   557.33                        0
                   799.35                   613.17
                   589.82                   716.66
```

## Bulk testing

`mmtest-h.py` evaluates compression and matrix–vector multiplication over a collection of
matrices.

```bash
./mmtest-h.py {mz|mm} [-b blocks] [-d dir] [-n num] [--files ...] [--sizes ...]
```

The input matrices and their dimensions are set inside `mmtest-h.py` through the global
variables `Files` and `Sizes`. `Files` is a list of input file names; `Sizes` is a dictionary
giving the number of rows and columns of each file. Entries of `Sizes` with no matching entry
in `Files` are ignored. Both defaults can be overridden with `--files` and `--sizes`.

### Compression

```bash
./mmtest-h.py mz -b 2 -d /data
```

Computes the CSRV and compressed representations of the matrices in `/data` and reports their
sizes as a percentage of the corresponding dense, uncompressed matrices. The `-b 2` option
partitions each matrix into 2 row blocks before computing its CSRV representation.

### Matrix–vector multiplication

```bash
./mmtest-h.py mm -b 2 -d /data -n num
```

Runs `num` iterations of the matrix–vector multiplication algorithms `csrvmm`, `re32mm`,
`reivmm` and `reansmm`, reporting the average time per iteration and the peak memory usage.
This assumes the matrices have already been partitioned into 2 row blocks and compressed as
above.

## Authors

* [Felipe Louza](https://github.com/felipelouza)
* [Giovanni Manzini](https://gitlab.com/manzai)
* [Guilherme Telles](https://github.com/gptelles)
