CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
INCLUDE_DIR = include
SRC_DIR = src

all: 

%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $^ -c -o $@

# routes: $(SRC_DIR)/routes.cc
# 	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean: