# Testing multithreaded matrix multiplication
gzip & uncompressed versions

## Compile

1) Run `cmake -DCMAKE_BUILD_TYPE=Release .` or `cmake -DCMAKE_BUILD_TYPE=Debug .`
2) Run `make -j 6` (optionally specifying `VERBOSE=1`)

## Run

Examples:
```bash
./test_gzip /data/tab_cla/gzip/census_b /data/tab_cla/gzip/y68.gz 2458285 68 16
./test_unco /data/tab_cla/unco/census_b /data/tab_cla/unco/y68 2458285 68 16
```