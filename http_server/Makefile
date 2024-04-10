CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread
INCLUDE_DIR = include
SRC_DIR = src

all: libhttp_server.a

%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $^ -c -o $@

libhttp_server.a: http_server.o client.o ../utils/utils.o
	ar rcs $@ $^

clean:
	rm *.o
	rm libhttp_server.a