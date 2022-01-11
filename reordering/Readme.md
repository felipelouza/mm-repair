# Column reordering subdirectory

## What is all about?

In this directory you find the code which generates the column similarity matrix, and the implementations of the following three heuristics for the column-reordering problem:

- PathCover
- PathCover+
- Maximum weighted matching (MWM)

In [this page](http://akira.ruc.dk/~keld/research/LKH-3/)[^1] you’ll find a recent Lin-Kernighan heuristic to the Travelling Salesman Problem (TSP), which is also of interest for our column-reordering problem.

---

## Prerequisites

Maximum Weighted Matching (MWM) requires the boost library (`sudo apt install libboost-all-dev`)

---

## Build 

Upon cloning the repository, from the root directory of the project `cd` into the `reordering` subdirectory.

```bash
cd reordering
```

A portion of the code base is written in `c++` and needs to be compiled. Run:
```bash
make -j
```
or, as an alternative, `make release -j` for better performances. The code will be compiled into a new `build` directory.

---

## Reordering in one shot 

Assume the matrix `covtype` has been already compressed into 3 row blocks (with the command `matrepair -b 3 somedir/covtype  581012 54` or with `mmtest.py` see main Readme file). Then, from the `reordering` subdirectory the command

```bash
reorder pc  somedir/covtype  581012 54 3
```
applies the *PathCover* reordering algorithm independently to each one of the three row blocks of matrix `covtype`. This command generates in the directory `somedir` a new set of files `covtype.3.0.vc`, `covtype.3.1.vc` and `covtype.3.2.vc` whose elements have been permuted using the *PathCover* heuristics. To compute the grammar compression of the reordered files go back to the main directory and use the command `matrepair -r -y -b3 -r somedir/covtype 581012 54` (the `-y` option skips the recomputation of the *.vc* files if they are more recent than the input file).

The command `reorder mwm ...` works as above using the *Maximum Weighted Matching* heuristics instead of *PathCover*. 

---

## Additional details on reordering

The following material explains the details of the reordering process and allows the use of other reordering algorithms.

## Summary tables

| Extension | Description |
| --- | --- | 
| `.csv` | A data set in CSV format of comma-separated numbers (either integers or floating-point). |
| `<DATA_SET>_cols`[^2] | A folder representing a matrix in column-major order. It contains a file for each column of the data set it refers to.  |
| `.tsp` | A file storing a column-similarity matrix (CSM). |
| `.par` | A configuration file used by the Lin–Kernighan (LKH) algorithm. |
| `.pairs` | A list of matched column pairs obtained via the maximum weighted matching (MWM) algorithm. |
| `.tsp.solution` | A column permutation obtained using the Lin–Kernighan (LKH) heuristic to the travelling salesman problem (TSP). |
| `.cover.solution` | A column permutation obtained using the PathCover algorithm. |
| `.cover2.solution` | A column permutation obtained using the PathCover+ algorithm. |
| `.mwm.solution` | A column permutation obtained using the maximum weighted matching (MWM) algorithm. |

| Program | Requirements | Output ext. | Description |
| --- | --- | --- | --- | 
| `column_major.py` | `.csv` | `<DATA_SET>_cols`[^2] | represents a given data set in column-major order, using a sparse representation. |
| `tsp_generator.cpp` | `<DATA_SET>_cols` | `.tsp`, `.par` | generates the all-pair CSM. |
| `tsp_generator_pruned_local.cpp` | `<DATA_SET>_cols` | `.tsp`, `.par` | generates the locally-pruned CSM. |
| `tsp_generator_pruned_global.cpp` | `<DATA_SET>_cols` | `.tsp`, `.par` | generates the globally-pruned CSM. |
| `cover.py` | `.tsp` | `.cover.solution` | computes the column permutation using PathCover. |
| `cover2.py` | `.tsp` | `.cover2.solution` | computes the column permutation using PathCover+. |
| `mwm.cpp` | `.tsp` | `.pairs` | computes the maximum weighted matching (MWM). |
| `mwm_sol_from_pairs.cpp` | `.pairs` | `.mwm.solution` | computes the column permutation using the maximum weighted matching (MWM). |


## Generating the column similarity matrix

### Step 1. Computing a column_major order representation
Given the `covtype` matrix in csv format (581012 rows, 54 columns), we firstly generate a sparse column_major representation for `covtype` using the following command:

```bash
python3 column_major.py <PATH_TO_COVTYPE>/covtype
```
This will generate the new folder `<PATH_TO_COVTYPE>/covtype_cols`.

### Step 2. All-pair column similarity matrix (CSM).
To compute the quadratic all-pair column similarity matrix (CSM) run the following command:
```bash
cd build && ./tsp_generator  <PATH_TO_COVTYPE>/covtype 581012 54
```
This will write the two configuration files `covtype.standard.tsp` and `covtype.standard.par` into the `<PATH_TO_COVTYPE>` folder.

### Step 2 (alternative). Pruned CSM.
In alternative, you may want to construct a pruned version of the CSM. Assuming you want to follow the _global pruning_ approach, run:
```bash
cd build && ./tsp_generator_pruned_global  <PATH_TO_COVTYPE>/covtype 581012 54 4
```
The last argument (equal to `4`, in our example) is the pruning parameter *k*. Smaller values of *k* translate into higher degrees of pruning. High values of *k* correspond instead to denser CSM (i.e., containg more edges) and to time-coonsuming CSM constructions.

The command in our example generates the two configuration files: `covtype.pruned_global_4.tsp` and `covtype.pruned_local_4.par`[^3].

For the _local pruning_ approach, on should use `./tsp_generator_pruned_local` instead. You’ll get a `covtype.pruned_local_4.tsp` and `covtype.pruned_local_4.par`[^3] files in this case.


## Executing the reordering algorithms
Let `<TSP_FILE>` denote any of the `.tsp` files generated following the instructions above (e.g., `covtype.standard.tsp`).

### Running PathCover
We need to go back to the `reordering` directory and launch the `cover.py` script.
Assuming that you are in in the `build` directory, run:
```bash
cd .. && python3 cover.py <PATH_TO_COVER>/<TSP_FILE>
```

### Running PathCover+
As for PathCover, but use `cover2.py` (rather than `cover.py`).

### Running the Maximum Weighed Matching (MWM)
Assuming that you are in the `build` directory, firstly run:
```bash
./mwm <PATH_TO_COVTYPE>/covtype <GENERATOR>
```
where <GENERATOR> stands for the method used to generate the CSM matrix, and is equal to:
- `standard`, for the all-to-all CSM.
- `pruned_global_4`, `pruned_global_8`, etc., for the globally-pruned CSM.
- `pruned_local_4`, `pruned_local_8`, etc., for the locally-pruned CSM.

Then, go back to the `reordering` directory, and launch the `mwm_sol_from_pairs.py` script:
```bash
cd .. && python3 mwm_sol_from_pairs.py <PATH_TO_COVTYPE>/covtype <GENERATOR>
```

---

[^1]: page visited in November 2021. 
[^2]: this is actually a folder name, rather than a file extension.
[^3]: the `.par` configuration files may be used to run the Lin-Kernighan algorithm.
