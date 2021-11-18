# Column reordering subdirectory

## What is all about?

In this directory you find the code which generates the column similarity matrix, and the implementations of the following three heuristics for the column-reordering problem:

- PathCover
- PathCover+
- Maximum weighed matching (MWM)

In [this page](http://akira.ruc.dk/~keld/research/LKH-3/) you’ll find a recent[^1] Lin-Kernighan heuristic to the Travelling Salesman Problem (TSP), which is also of interest for our column-reordering problem.


## Build 

Upon cloning the repository, from the root directory of the project `cd` into the this subdirectory.

```bash
cd algos
```

A portion of teh code base is written in `c++` and needs to be compiled. Run:
```bash
make -j
```
or, in alternative, `make release -j` for better performances. The code will be compiled into a `build` directory.

## Generating the column similarity matrix

### Step 1. Computing a column-major order representation
Given the `covtype` matrix in csv format (581012 rows, 54 columns), we firstly generate a sparse column-major representation for `covtype` using the following command.

```bash
python3 column-major.py <PATH_TO_COVTYPE>/covtype
```
This script will generate the new folder `<PATH_TO_COVTYPE>/covtype_cols`.

### Step 2. All-pair column similarity matrix (CSM).
To compute the quadratic all-pair column similarity matrix (CSM) run the following command:
```bash
cd build && ./tsp_generator  <PATH_TO_COVTYPE>/covtype 581012 54
```
This command will write the two configuration files `covtype.standard.tsp` and `covtype.standard.par` into the `<PATH_TO_COVTYPE>` folder.

### Step 2 (alternative). Pruned CSM.
In alternative, you may want to construct a pruned version of the CSM. Assuming you want to follow the _global pruning_ approach, run:
```bash
cd build && ./tsp_generator_pruned_global  <PATH_TO_COVTYPE>/covtype 581012 54 4
```
which generates the two configuration files `covtype.pruned_global_4.tsp` and `covtype.pruned_local_4.par`[^2].

For the local pruning approach, use `./tsp_generator_pruned_local` instead. You’ll get a `covtype.pruned_local_4.tsp` and `covtype.pruned_local_4.par`[^2] files.


## Executing the reordering algorithms
Let `<TSP_FILE>` denote any of the `.tsp` files generated following the isntructions above (e.g., `covtype.standard.tsp`).

### Running PathCover
We need to back to the `algos` directory and launch the `cover.py` script.
Assuming that you are in in the `build` directory, run:
```bash
cd .. && python3 cover.py <PATH_TO_COVER>/<TSP_FILE>
```

### Running PathCover+
As for PathCover, but use `cover2.py` (rather than `cover.py`).

### Running the Maximum Weighed Matching (MWM)
Assuming that you are in the `build` directory firstly run:
```bash
./mwm <PATH_TO_COVTYPE>/covtype <GENERATOR>
```
where <GENERATOR> stands for the method used to generate the CSM matrix, and is equal to:
- `standard`, for the all-to-all CSM.
- `pruned_global_4`, `pruned_global_8`, etc., for the globally-pruned CSM.
- `pruned_local_4`, `pruned_local_8`, etc., for the locally-pruned CSM.

Then go back to the `algos` directory, and launch the `mwm_sol_from_pairs.py` script:
```bash
cd .. && python3 mwm_sol_from_pairs.py <PATH_TO_COVTYPE>/covtype <GENERATOR>
```

---

[^1]: page visited in November 2021. 
[^2]: the `.par` configuration files may be used to run the Lin-Kernighan algorithm.
