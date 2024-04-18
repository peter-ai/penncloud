CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
LDFLAGS = -I/opt/homebrew/opt/openssl@3/include -L../http_server -lhttp_server -L/opt/homebrew/opt/openssl@3/lib -lcrypto

TARGETS = frontend_main

all: $(TARGETS)

frontend_main: ./src/frontend_main.cc ./utils/src/fe_utils.cc ../http_server/src/http_server.cc ./src/authentication.cc ./src/mailbox.cc ./src/drive.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	rm -f *o

clean:
	rm -f frontend_main