#!/bin/bash
set -e

echo "Compiling tests..."
g++ -I./tests -I./lte-cam tests/test_sleep_math.cpp lte-cam/sleep_math.cpp -o run_tests

echo "Running tests..."
./run_tests

echo "Cleaning up..."
rm run_tests

echo "All tests passed successfully!"
