CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -I/opt/homebrew/opt/openssl@3/include -I/opt/homebrew/opt/ldns/include
LDFLAGS = -L../http_server -lhttp_server -L/opt/homebrew/opt/openssl@3/lib -lcrypto -L/opt/homebrew/opt/ldns/lib -lldns

TARGETS = frontend_main

all: $(TARGETS)

frontend_main: ./src/frontend_main.cc ./utils/src/fe_utils.cc ../http_server/libhttp_server.a ./src/authentication.cc ./src/response_codes.cc ./src/mailbox.cc ./src/drive.cc ./src/smtp_client.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	rm -f *o

clean:
	rm -f frontend_main