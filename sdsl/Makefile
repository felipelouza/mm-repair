# if present read helper from sdsl directory
Helper=Make.helper
ifneq ($(wildcard $(Helper)),)
include $(Helper)
endif
# flags and targets
CXX_FLAGS=-std=c++11 -Wall -I$(INC_DIR) -L$(LIB_DIR)
CCLIB=-lsdsl
SOURCES=$(wildcard *.cpp)
EXECS=$(SOURCES:.cpp=.x)

all: $(EXECS)
	        
%.x:%.cpp
	$(CXX) $(CXX_FLAGS) -o $@ $< $(CCLIB) 

clean:
	rm -f $(EXECS)
	rm -rf *.dSYM

