#!/bin/bash
set -e

echo "Cleaning up old coverage data..."

find build -name "*.gcda" -type f -delete

echo "Building project..."
cmake --build build

echo "Running tests..."
./build/tests/unit_tests

echo "Generating coverage report..."
rm -rf ./coverage_report
grcov . -s . --binary-path ./build/ -t html -o ./coverage_report