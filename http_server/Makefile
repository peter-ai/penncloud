CXX=g++
CXXFLAGS=-std=c++11 -Wall -Wextra -pthread
SRC_DIR=src
INCLUDE_DIR=include
BUILD_DIR=build

# Find all .cc source files in SRC_DIR
SRCS:=$(wildcard $(SRC_DIR)/*.cc)

# Generate corresponding .o object filenames in BUILD_DIR
OBJS:=$(patsubst $(SRC_DIR)/%.cc,$(BUILD_DIR)/%.o,$(SRCS))

# Rule to compile .cc files into .o files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

test.o: test.cc
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

test: test.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Phony target to clean the build directory
clean:
	rm -rf $(BUILD_DIR)
	rm -f test test.o

# CXX=g++
# CXXFLAGS=-std=c++11 -Wall -Wextra
# INCLUDE_DIRS=http_server/include
# LIB_DIRS=http_server
# LIBS=http_server/http_server.a

# test.o: test.cc
# 	$(CXX) $(CXXFLAGS) $(addprefix -I, $(INCLUDE_DIRS)) -c $< -o $@

# # Main target rule to build the executable
# test: test.o $(LIBS)
# 	$(CXX) $(CXXFLAGS) -o $@ $^

# # Phony target to clean the main executable and object files
# clean:
# 	rm -f test test.o
# 	for dir in $(LIB_DIRS); do \
# 		$(MAKE) -C $$dir clean; \
# 	done
    
# # Include module-specific Makefiles
# .PHONY: http_server
# http_server:
# 	$(MAKE) -C http_server