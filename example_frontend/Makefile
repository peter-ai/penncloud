CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
LDFLAGS = -L../http_server -lhttp_server
INCLUDE_DIR = include
SRC_DIR = src

all: routes

%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $^ -c -o $@

routes: $(SRC_DIR)/routes.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f routes


# if you have separate handler files, the above Makefile could be changed to look something like this:
# CXX = g++
# CXXFLAGS = -std=c++11 -Wall -Wextra
# LDFLAGS = -Lhttp_server -lhttp_server
# INCLUDE_DIR = include
# SRC_DIR = src

# # place any other .cc that you want to compile into .o files here
# # for example, I have a route handler file called mail_routes.cc, I'd add it to the line below as mail_routes.o
# all: handler1.o handler2.o routes.o

# %.o: $(SRC_DIR)/%.cc
# 	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) $^ -c -o $@

# routes: routes.o handler1.o handler2.o ../utils/utils.o
# 	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# clean:
# 	rm -f *.o
