CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
INCLUDE_DIR = include
SRC_DIR = src

# ! removed kvs_client.o for testing
all: tablet.o backend_server.o backend_main

%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $^ -c -o $@

# ! removed kvs_client.o for testing
backend_main: tablet.o backend_server.o ../utils/utils.o $(SRC_DIR)/backend_main.cc 
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.o
	rm -f backend_main