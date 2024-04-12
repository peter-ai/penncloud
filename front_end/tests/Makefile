TARGETS = auth_test

all: $(TARGETS)

auth_test: auth_test.cc
	g++ $^ -o $@ -lssl -lcrypto