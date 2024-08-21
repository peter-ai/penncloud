CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
LDFLAGS = -I/opt/homebrew/opt/openssl@3/include -L../http_server -lhttp_server -L/opt/homebrew/opt/openssl@3/lib -lcrypto

TARGETS = relay_main

all: $(TARGETS)

relay_main: ./src/relay_main.cc ./src/smtp_server.cc ../utils/src/utils.cc ../front_end/utils/src/fe_utils.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	rm -f *o

clean:
	rm -f relay_main