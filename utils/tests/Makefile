TARGETS = logger_test

all: $(TARGETS)

logger_test: logger_test.cc ../src/utils.cc
	g++ -pthread $^ -o $@