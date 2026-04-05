CXX = g++
CXXFLAGS = -I../lte-cam -Wall -Wextra

all: test_sleep_math

test_sleep_math: test_sleep_math.cpp ../lte-cam/sleep_math.h
	$(CXX) $(CXXFLAGS) test_sleep_math.cpp -o test_sleep_math

run: test_sleep_math
	./test_sleep_math

clean:
	rm -f test_sleep_math
