CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
INCLUDE_DIR = include
SRC_DIR = src

all: utils.o

%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $^ -c -o $@
	rm -f *.o

clean:
	rm -f *.o