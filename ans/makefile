# compilation flags
CXX_FLAGS=-std=c++17 -O3 -g
CFLAGS=-O3 -Wall -std=c99 -g
CC=gcc

# main executables 
EXECS=ansfold.x encode.x decode.x

# targets not producing a file declared phony
.PHONY: all clean

all: $(EXECS)


ansfold.x: ansfold.cpp cutil.hpp util.hpp methods.hpp
	$(CXX) $(CXX_FLAGS) -o $@ $< -lboost_system -lboost_regex -lboost_filesystem -lboost_program_options

encode.x: encode.cpp
	$(CXX) $(CXX_FLAGS) -o $@ $< 

decode.x: decode.cpp
	$(CXX) $(CXX_FLAGS) -o $@ $< 



%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(EXECS) *.o 
