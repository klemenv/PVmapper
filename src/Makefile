CXX = g++
CXX_FLAGS = -Wfatal-errors -Wall -Wextra -Wconversion -Wshadow -std=c++17
#CXX_FLAGS += -g -ggdb -O0 -DDEBUG

# Binary name
BIN = pvmapper
# Put all auto generated stuff to this build dir.
BUILD_DIR = ../build

# List of all .cpp source files.
CPP = main.cpp $(wildcard *.cpp)

# All .o files go to build dir.
OBJ = $(CPP:%.cpp=$(BUILD_DIR)/%.o)
# Gcc/Clang will create these .d files containing dependencies.
DEP = $(OBJ:%.o=%.d)

# Default target named after the binary.
$(BIN) : $(BUILD_DIR)/$(BIN)

# Actual target of the binary - depends on all .o files.
$(BUILD_DIR)/$(BIN) : $(OBJ)
	@mkdir -p $(@D)
	$(CXX) $(CXX_FLAGS) $^ -o $@

# Include all .d files
-include $(DEP)

# Build target for every single object file.
# The potential dependency on header files is covered
# by calling `-include $(DEP)`.
$(BUILD_DIR)/%.o : %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXX_FLAGS) -MMD -c $< -o $@

.PHONY : clean
clean :
	-rm -fR $(BUILD_DIR)
