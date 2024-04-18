CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
INCLUDE_DIR = include
SRC_DIR = src

all: backend_main

%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $^ -c -o $@

backend_main: tablet.o kvs_client.o backend_server.o ../utils/utils.o $(SRC_DIR)/backend_main.cc 
	$(CXX) $(CXXFLAGS) $^ -o $@
	rm -f *.o

clean:
	rm -f *.o
	rm -f backend_main