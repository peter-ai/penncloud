CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
LDFLAGS = -I/opt/homebrew/opt/openssl@3/include -L../http_server -lhttp_server -L/opt/homebrew/opt/openssl@3/lib -lcrypto

TARGETS = relay_main

all: $(TARGETS)

loadbalancer_main: ./src/relay_main.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	rm -f *o

clean:
	rm -f relay_main