CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
INCLUDE_DIR = include
SRC_DIR = src

all: tablet.o kvs_client.o backend_server.o backend_main

%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $^ -c -o $@

backend_main: $(SRC_DIR)/backend_main.cc tablet.o kvs_client.o backend_server.o ../utils/utils.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f backend_main