CXX      := -g++
CXXFLAGS := -std=c++11
LDFLAGS  := -lboost_system
BUILD    := ./build
OBJ_DIR  := $(BUILD)/objects
APP_DIR  := $(BUILD)
INCLUDE  := #-I ../include/boost_1_71_0/
SRC      := ${TARGET}.cpp

OBJECTS := $(OBJ_DIR)/mwm.o $(OBJ_DIR)/tsp_generator.o $(OBJ_DIR)/tsp_generator_pruned_global.o $(OBJ_DIR)/tsp_generator_pruned_local.o 
TARGETS = mwm ring

# $(APP_DIR)/mwm
all: build  $(APP_DIR)/tsp_generator $(APP_DIR)/tsp_generator_pruned_global $(APP_DIR)/tsp_generator_pruned_local

$(OBJ_DIR)/mwm.o: mwm.cpp
		@mkdir -p $(@D)
		$(CXX) $(INCLUDE) $(LDFLAGS) -o $@ -c $< $(CXXFLAGS)

$(OBJ_DIR)/tsp_generator.o: tsp_generator.cpp
		@mkdir -p $(@D)
		$(CXX) $(INCLUDE) $(LDFLAGS) -o $@ -c $< $(CXXFLAGS)

$(OBJ_DIR)/tsp_generator_pruned_global.o: tsp_generator_pruned_global.cpp
		@mkdir -p $(@D)
		$(CXX) $(INCLUDE) $(LDFLAGS) -o $@ -c $< $(CXXFLAGS)

$(OBJ_DIR)/tsp_generator_pruned_local.o: tsp_generator_pruned_local.cpp
		@mkdir -p $(@D)
		$(CXX) $(INCLUDE) $(LDFLAGS) -o $@ -c $< $(CXXFLAGS)

#

$(APP_DIR)/mwm: $(OBJ_DIR)/mwm.o
		@mkdir -p $(@D)
		$(CXX) $(INCLUDE) $(LDFLAGS) -o $@ $< $(CXXFLAGS) $(LDFLAGS)

$(APP_DIR)/tsp_generator: $(OBJ_DIR)/tsp_generator.o
		@mkdir -p $(@D)
		$(CXX) $(INCLUDE) $(LDFLAGS) -o $@ $< $(CXXFLAGS) $(LDFLAGS)

$(APP_DIR)/tsp_generator_pruned_global: $(OBJ_DIR)/tsp_generator_pruned_global.o
		@mkdir -p $(@D)
		$(CXX) $(INCLUDE) $(LDFLAGS) -o $@ $< $(CXXFLAGS) $(LDFLAGS)

$(APP_DIR)/tsp_generator_pruned_local: $(OBJ_DIR)/tsp_generator_pruned_local.o
		@mkdir -p $(@D)
		$(CXX) $(INCLUDE) $(LDFLAGS) -o $@ $< $(CXXFLAGS) $(LDFLAGS)

.PHONY: all build clean debug release

build:
		@mkdir -p $(APP_DIR)
		@mkdir -p $(OBJ_DIR)

debug: CXXFLAGS += -DDEBUG -g
debug: all

release: CXXFLAGS += -O3
release: all

clean:
		-@rm -rv $(BUILD)
