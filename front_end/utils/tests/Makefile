TARGETS = fe_utils_test simpleserver

all: $(TARGETS)

fe_utils_test: fe_utils_test.cc ../src/fe_utils.cc
	g++ -std=c++11 -pthread $^ -o $@

simpleserver: simpleserver.cc
	g++ -std=c++11 -pthread $^ -o $@