#!/bin/bash
set -euo pipefail

if [[ ! -f "input.txt" ]]; then
	echo "Error: input file not found: input.txt"
	exit 1
fi

echo "Configuring project..."
cmake -S . -B build -DENABLE_COVERAGE=OFF

echo "Building CLI..."
cmake --build build --target egraph_cli

echo "Running CLI with input.txt..."
build/src/egraph_cli < "input.txt"
