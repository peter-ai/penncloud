CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
INCLUDE_DIR = include
SRC_DIR = src

all: backend_main

%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $^ -c -o $@

backend_main: $(SRC_DIR)/backend_main.cc ../utils/utils.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f backend_main