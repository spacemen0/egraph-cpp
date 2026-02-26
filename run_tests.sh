#!/bin/bash
set -e

echo "Cleaning up old data..."
rm -rf ./coverage_report
rm -rf ./*.svg
find build -name "*.gcda" -type f -delete

echo "Building project..."
cmake --build build

echo "Running tests..."
./build/tests/unit_tests

echo "Generating coverage report..."
grcov . -s . --binary-path ./build/ -t html -o ./coverage_report