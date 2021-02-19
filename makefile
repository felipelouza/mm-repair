# compilation flags
CFLAGS=-g -Wall -std=gnu99
CC=gcc 
# comment out definition to get rid of malloc_count 
MALLOC_FLAGS=mallocc/malloc_count.c -DMALLOC_COUNT -ldl

# executables in this directory
EXECS=remult remultf remm

# targets not producing a file declared phony
.PHONY: all brepair clean

all: $(EXECS) brepair

# general rule for the targets in this directory
%: %.c
	gcc $(CFLAGS) -o $@ $< 

remm: remm.c rematrix.h
	gcc $(CFLAGS) -o $@ $< $(MALLOC_FLAGS)

# special rule for remultf 
remultf: remult.c
	gcc $(CFLAGS) -o $@ $< -DFLOAT_VALS

brepair:
	make -C brepair

clean:
	rm -f $(EXECS)
	make -C brepair clean

