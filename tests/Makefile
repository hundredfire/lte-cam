CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -I../lte-cam

TEST_SRC = test_sleep_calculations.cpp
TEST_OUT = test_sleep_calculations

all: test

test: $(TEST_OUT)
	./$(TEST_OUT)

$(TEST_OUT): $(TEST_SRC) ../lte-cam/sleep_calculations.h
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $(TEST_OUT)

clean:
	rm -f $(TEST_OUT)

.PHONY: all test clean
