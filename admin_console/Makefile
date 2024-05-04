CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
LDFLAGS = -I/opt/homebrew/opt/openssl@3/include -L../http_server -lhttp_server -L/opt/homebrew/opt/openssl@3/lib -lcrypto

TARGETS = admin_main

all: $(TARGETS)

admin_main: ./src/admin_main.cc ../front_end/utils/src/fe_utils.cc ../http_server/src/http_server.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	rm -f *o

clean:
	rm -f  