

CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
LDFLAGS = -I/opt/homebrew/opt/openssl@3/include -L../http_server -lhttp_server -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto

TARGETS = main

all: $(TARGETS)

# auth_routes: ./tests/auth_routes.cc ./src/authentication.cc ./src/main.cc ./utils/src/fe_utils.cc ../http_server/src/http_server.cc
# 	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

main: ./src/main.cc ./utils/src/fe_utils.cc ../http_server/src/http_server.cc ./src/authentication.cc ./src/mailbox.cc ./src/drive.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f main