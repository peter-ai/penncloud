CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
LDFLAGS = -Lhttp_server -lhttp_server

test: test.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)