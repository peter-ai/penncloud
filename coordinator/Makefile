CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -pthread

TARGETS = coordinator

all: $(TARGETS)

coordinator: ./src/coordinator.cc ../utils/src/utils.cc
	$(CXX) $(CXXFLAGS) $^ -o $@
	rm -f *.o

clean:
	rm -f coordinator