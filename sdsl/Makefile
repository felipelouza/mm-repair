#include Make.helper
# CXX_FLAGS=$(MY_CXX_FLAGS) $(MY_CXX_OPT_FLAGS) -I$(INC_DIR) -L$(LIB_DIR) 
LIB_DIR = /home/giovanni/c/lib
INC_DIR = /home/giovanni/c/include
CXX_FLAGS=-std=c++11 -Wall -Wextra -I$(INC_DIR) -L$(LIB_DIR)
CCLIB=-lsdsl
SOURCES=$(wildcard *.cpp)
EXECS=$(SOURCES:.cpp=.x)

all: $(EXECS)
	        
%.x:%.cpp
	$(CXX) $(CXX_FLAGS) -o $@ $< $(CCLIB) 

clean:
	rm -f $(EXECS)
	rm -rf *.dSYM

