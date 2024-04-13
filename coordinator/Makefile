TARGETS = coordinator

all: $(TARGETS)

coordinator: ./src/coordinator.cc ../utils/src/utils.cc
	g++ -std=c++11 $^ -o $@