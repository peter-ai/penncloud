CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
LDFLAGS = -I/opt/homebrew/opt/openssl@3/include -L../http_server -lhttp_server -L/opt/homebrew/opt/openssl@3/lib -lcrypto

TARGETS = loadbalancer_main

all: $(TARGETS)

loadbalancer_main: ./src/loadbalancer_main.cc ./src/loadbalancer.cc ../utils/src/utils.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	rm -f *o

clean:
	rm -f loadbalancer_main