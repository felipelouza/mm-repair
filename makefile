# compilation flags
CFLAGS=-g -Wall -std=c99
CC=gcc 
# comment out this definition to get rid of malloc_count 
MALLOC_FLAGS=mc/malloc_count.c -DMALLOC_COUNT -ldl

# executables in this directory
EXECS=remm recg csrmm

# malloc_count dedendencies for 
ifdef MALLOC_FLAGS
MALLOC_FILES=mc/malloc_count.c mc/malloc_count.h
endif

# targets not producing a file declared phony
.PHONY: all brepair clean

# main target
all: $(EXECS) brepair

# general rule for the targets in this directory
%: %.c
	gcc $(CFLAGS) -o $@ $< 


# single matrix vector multiplication, double entries
# one could create versions for float and int defining FLOAT_VAL or INT_VALS 
remm: remm.c rematrix.h vector.h $(MALLOC_FILES)
	gcc $(CFLAGS) -o $@ $< $(MALLOC_FLAGS)

csrmm: remm.c csrmatrix.h vector.h $(MALLOC_FILES)
	gcc $(CFLAGS) -o $@ $< $(MALLOC_FLAGS) -DCSR_MATRIX

# single matrix vector multiplication, double entries
# one could create versions for float and int defining FLOAT_VAL or INT_VALS 
recg: recg.c rematrix.h vector.h $(MALLOC_FILES)
	gcc $(CFLAGS) -o $@ $< -lm $(MALLOC_FLAGS)


# remult was a first, unstructured, prototype using float entries that 
# can be created by the general rule above with target remult 
# the following rule creates the version using integers instead of floats
# if necessary the flag INT_VALS can be used also for the other tools 
iremult: remult.c
	gcc $(CFLAGS) -o $@ $< -DINT_VALS

# directory containing the (balanced) irepar/idespair tools 
brepair:
	make -C brepair

clean:
	rm -f $(EXECS)
	make -C brepair clean

