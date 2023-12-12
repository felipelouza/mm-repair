# Default position of the lib and include files for the SDSL-lite library; 
# change this definition if they are somewhere else
LIB_DIR = ${HOME}/lib
INC_DIR = ${HOME}/include

# Compilation flags
CFLAGS=-g -Wall -std=c99 -O3
CC=gcc 
CXX_FLAGS=-std=c++17 -g -DNDEBUG -O3 -msse4.2

# comment out this definition to get rid of malloc_count 
# MALLOC_FLAGS=tools/malloc_count.c -DMALLOC_COUNT -ldl

# executables in this directory
EXECS=re32mm csrvmm reansmm reivmm reans32mm csvmat2csrv bin2csrv bin2csrvf bin2csrvi

# malloc_count dependencies
ifdef MALLOC_FLAGS
MALLOC_FILES=tools/malloc_count.c tools/malloc_count.h
endif

# targets not producing a file declared phony
.PHONY: all brepair ans sdsl clean

# main target
all: $(EXECS) brepair ansf sdsl

# general rules for the targets in this directory
%: %.c
	$(CC) $(CFLAGS) -o $@ $< 
%: %.cpp
	$(CXX) $(CXX_FLAGS) -o $@ $< 

# special rules for the bin2csrv variants
bin2csrvf: bin2csrv.cpp
	$(CXX) $(CXX_FLAGS) -o $@ $< -DTypecode=2
bin2csrvi: bin2csrv.cpp
	$(CXX) $(CXX_FLAGS) -o $@ $< -DTypecode=1


# left and right matrix vector multiplication, double entries
# one could create versions for float and int defining FLOAT_VAL or INT_VALS 
re32mm: remm.c rematrix.h vector.h tools/xerrors.h $(MALLOC_FILES)
	$(CC) $(CFLAGS) -o $@ $< $(MALLOC_FLAGS) -pthread

csrvmm: remm.c csrmatrix.h vector.h $(MALLOC_FILES)
	$(CC) $(CFLAGS) -o $@ $< $(MALLOC_FLAGS) -DCSR_MATRIX -pthread

reans32mm: remm.c rematrix.h vector.h ans/decode.hpp $(MALLOC_FILES)
	$(CXX) $(CXX_FLAGS) -o $@ $< $(MALLOC_FLAGS) -DUSE_ANS -pthread

reivmm: remm.c rematrix.hpp vector.h $(MALLOC_FILES)
	$(CXX) $(CXX_FLAGS) -o $@ $< $(MALLOC_FLAGS) -I$(INC_DIR) -L$(LIB_DIR) -lsdsl -pthread  -DUSE_INTVEC

reansmm: remm.c rematrix.hpp vector.h ans/decode.hpp $(MALLOC_FILES)
	$(CXX) $(CXX_FLAGS) -o $@ $< $(MALLOC_FLAGS) -I$(INC_DIR) -L$(LIB_DIR) -lsdsl -pthread -DUSE_ANSIV 


# conjugate gradient method (no longer updated)
recg: recg.c rematrix.h vector.h $(MALLOC_FILES)
	$(CC) $(CFLAGS) -o $@ $< -lm $(MALLOC_FLAGS)

csrcg: recg.c csrmatrix.h vector.h $(MALLOC_FILES)
	$(CC) $(CFLAGS) -o $@ $< $(MALLOC_FLAGS) -DCSR_MATRIX



# directory containing the (balanced) irepar/idespair tools 
brepair:
	make -C brepair

# directory containing the ans-fold encoding-decoding tools 
ansf:
	make -C ans

# directory containing the integer vector enconding-decoding tools 
sdsl:
	make -C sdsl


clean:
	rm -f $(EXECS)
	make -C brepair clean
	make -C ans clean
	make -C sdsl clean

