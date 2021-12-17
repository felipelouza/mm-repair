# if present read helper from sdsl directory
Helper=sdsl/Make.helper
ifneq ($(wildcard $(Helper)),)
include $(Helper)
endif
# compilation flags
CFLAGS=-g -Wall -std=c99 -O3
CC=gcc 
CXX_FLAGS=-std=c++17 -DNDEBUG -O3 -msse4.2


# comment out this definition to get rid of malloc_count 
MALLOC_FLAGS=tools/malloc_count.c -DMALLOC_COUNT -ldl

# executables in this directory
EXECS=re32mm csrvmm reansmm reivmm reans32mm vc_reorder.x

# malloc_count dedendencies for 
ifdef MALLOC_FLAGS
MALLOC_FILES=tools/malloc_count.c tools/malloc_count.h
endif

# targets not producing a file declared phony
.PHONY: all brepair ans sdsl clean

# main target
all: $(EXECS) brepair ansf sdsl

# general rule for the targets in this directory
%: %.c
	$(CC) $(CFLAGS) -o $@ $< 


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

vc_reorder.x: vc_reorder.cpp
	$(CXX) $(CXX_FLAGS) -o $@ $<


# conjugate gradient method
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

# directory containing the integer vector econding decoding tools 
sdsl:
	make -C sdsl


clean:
	rm -f $(EXECS)
	make -C brepair clean
	make -C ans clean
	make -C sdsl clean

