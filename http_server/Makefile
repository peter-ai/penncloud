CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -pthread
INCLUDE_DIR = include
SRC_DIR = src

all: libhttp_server.a

%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $^ -c -o $@

libhttp_server.a: http_server.o client.o ../utils/utils.o
	ar rcs $@ $^

clean:
	rm -f *.o
	rm -f libhttp_server.a