CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
LDFLAGS = -I/opt/homebrew/opt/openssl@3/include -L../http_server -lhttp_server -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto

TARGETS = auth_routes

all: $(TARGETS)

auth_routes: ./tests/auth_routes.cc ./src/authentication.cc ./utils/src/fe_utils.cc ../http_server/src/http_server.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f auth_routes