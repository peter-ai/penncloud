CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra

TARGETS = coordinator

all: $(TARGETS)

coordinator: ./src/coordinator.cc ../utils/src/utils.cc
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -f coordinator