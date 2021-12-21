# Default position of the lib and include files for the SDSL-lite library; 
# change this definition if they are in a non-standard location
LIB_DIR = ${HOME}/lib
INC_DIR = ${HOME}/include


# Compilation flags and targets
CXX_FLAGS=-std=c++11 -O3 -Wall -I$(INC_DIR) -L$(LIB_DIR)
CCLIB=-lsdsl
SOURCES=$(wildcard *.cpp)
EXECS=$(SOURCES:.cpp=.x)


all: $(EXECS)
	        
%.x:%.cpp
	$(CXX) $(CXX_FLAGS) -o $@ $< $(CCLIB) 

clean:
	rm -f $(EXECS)
	rm -rf *.dSYM

