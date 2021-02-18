# compilation flags
CFLAGS=-g -Wall -std=gnu99
CC=gcc 

# executables in this directory
EXECS=remult remultf

# targets not producing a file declared phony
.PHONY: all brepair clean

all: $(EXECS) brepair

# general rule for the targets in this directory
%: %.c
	gcc $(CFLAGS) -o $@ $< 

# special rule for remultf 
remultf: remult.c
	gcc $(CFLAGS) -o $@ $< -DFLOAT_VALS

brepair:
	make -C brepair

clean:
	rm -f $(EXECS)
	make -C brepair clean

