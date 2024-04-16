TARGETS = fe_utils_test simpleserver

all: $(TARGETS)

# Update the include path and add utils.cc to the compilation
fe_utils_test: fe_utils_test.cc ../src/fe_utils.cc ../../../utils/src/utils.cc
	g++ -std=c++11 -pthread -I../../../utils/include $^ -o $@

simpleserver: simpleserver.cc
	g++ -std=c++11 -pthread $^ -o $@