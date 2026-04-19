#!/bin/bash
set -euo pipefail

mode="${1:-0}"
input_file=""

if [[ "$mode" == "0" ]]; then
	input_file="input.txt"
elif [[ "$mode" == "1" ]]; then
	input_file="input_symbolic.txt"
else
	echo "Usage: $0 [0|1]"
	echo "  0 -> run with input.txt"
	echo "  1 -> run with input_symbolic.txt"
	exit 1
fi

if [[ ! -f "$input_file" ]]; then
	echo "Error: input file not found: $input_file"
	exit 1
fi

echo "Configuring project..."
cmake -S . -B build -DENABLE_COVERAGE=OFF

echo "Building CLI..."
cmake --build build --target egraph_cli

echo "Running CLI with $input_file..."
build/src/egraph_cli < "$input_file"
