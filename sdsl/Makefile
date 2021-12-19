# Add here the position of the lib and include files for 
# the SDSL-lite library if they are in a non-standard location
# LIB_DIR = /home/user/c/lib
# INC_DIR = /home/user/c/include

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

