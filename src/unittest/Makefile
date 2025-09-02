CXX = g++
CXX_FLAGS = -Wfatal-errors -Wall -Wextra -Wconversion -Wshadow -std=c++17
CXX_FLAGS += -g -ggdb -O0 -I..

# Enable using resolv library for to obtain TTL for DNS records (only works on POSIX systems)
CXX_FLAGS += -DUSE_LIB_RESOLVE
LD_FLAGS = -lresolv

# Binary name
BIN = unittests
# Put all auto generated stuff to this build dir.
BUILD_DIR = ../../build

# List of all .cpp source files.
CPP = $(wildcard *.cpp)

# All .o files go to build dir.
OBJ = $(CPP:%.cpp=$(BUILD_DIR)/%.o)
LIB_OBJ = $(filter-out $(BUILD_DIR)/main.o, $(wildcard $(BUILD_DIR)/*.o))
# Gcc/Clang will create these .d files containing dependencies.
DEP = $(OBJ:%.o=%.d)

# Default target named after the binary.
$(BIN) : $(BUILD_DIR)/$(BIN)

# Actual target of the binary - depends on all .o files.
$(BUILD_DIR)/$(BIN) : $(OBJ) $(LIB_OBJ)
	@mkdir -p $(@D)
	$(CXX) $(CXX_FLAGS) $^ -o $@ $(LD_FLAGS)

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
