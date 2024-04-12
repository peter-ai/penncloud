TARGETS = auth_test

all: $(TARGETS)

auth_test: auth_test.cc
	g++ -std=c++11 $^ -I/opt/homebrew/opt/openssl@3/include -L/opt/homebrew/opt/openssl@3/lib -o $@ -lssl -lcrypto