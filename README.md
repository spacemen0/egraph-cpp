# egraph-cpp

A C++ implementation of an [e-graph](https://egraphs-good.github.io/) (equality graph) data structure, focused on optimizing **matrix expressions** using equality saturation.

## Overview

E-graphs compactly represent many equivalent expressions at once. This project uses e-graphs to discover algebraically simpler or computationally cheaper forms of linear-algebra expressions (e.g. eliminating redundant inverses, exploiting orthogonality, or choosing a cheaper matrix factorization).

The project ships:

- A **core e-graph library** (e-nodes, e-classes, union-find, pattern matching, rewriting, extraction).
- An **interactive CLI** (`egraph_cli`) that accepts commands through a simple REPL.

## Requirements

| Tool | Minimum version |
|------|----------------|
| CMake | 3.15 |
| C++ compiler | C++20 (GCC 10+, Clang 12+) |
| GoogleTest | fetched automatically via CMake `FetchContent` |

## Building

```bash
# Configure (Release mode)
cmake -S . -B build

# Build the CLI
cmake --build build --target egraph_cli

# Build everything including tests
cmake --build build
```

For a **Debug** build with AddressSanitizer:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

For a build with **coverage instrumentation**:

```bash
cmake -S . -B build -DENABLE_COVERAGE=ON
cmake --build build
```

## Running the CLI

```bash
./build/src/egraph_cli
```

You can also pipe a script file into the CLI:

```bash
./build/src/egraph_cli < input.txt
```

A convenience wrapper is provided:

```bash
./run_cli.sh   # builds the project and runs it with input.txt
```

### REPL commands

| Command | Description |
|---------|-------------|
| `parse <expr>` | Parse and store an expression |
| `add properties <prop...>` | Attach matrix properties to variables |
| `add rule-set <name...>` | Enable one or more rewrite rule sets |
| `rewrite <N\|null>` | Apply rewrites for N iterations, or until saturation (`null`) |
| `extract <id>` | Extract the lowest-cost equivalent expression for the given id |
| `show [state\|available-rule-sets]` | Print session state or list available rule sets |
| `reset` | Clear the session |
| `help` | Print command reference |
| `quit` / `exit` | Exit the REPL |

## Expression syntax

Expressions are written in **prefix S-expression** notation:

```
Mul(A, B)            # matrix multiply A × B
Tr(A)                # transpose
Inv(A)               # inverse
Add(A, B)            # addition
Neg(A)               # negation
Get(QR(A), 0)        # extract component from a factorization
```

Leaf nodes are plain identifiers (variable names or literal symbols such as `Identity`).

## Matrix properties

Properties are attached before rewriting so that condition-guarded rules can fire:

```
add properties X:Matrix(100x20)[tall]  y:Matrix(100x1)
```

Property format: `<name>:Matrix(<rows>x<cols>)[flag1][flag2]...`

Available flags: `tall`, `square`, `symmetric`, `positive-definite`, `orthogonal`, `orthonormal`, `singular`.

## Available rule sets

| Name | What it contains |
|------|-----------------|
| `algebraic` | Associativity of `Mul`, transpose of a product, identity elimination |
| `inverse` | Inverse cancellation, inverse of a product |
| `orthogonality` | `Tr(A) × A = I` for orthogonal / orthonormal matrices |
| `factorization` | QR, LU, and Cholesky (LLt) decomposition-based inversion rules |
| `zero-negation` | Double negation, zero absorption |

Multiple rule sets can be enabled in a single `add rule-set` command.

## Example session

The following session (also in `input.txt`) simplifies the ordinary least-squares estimator
`(X^T X)^{-1} X^T y`:

```
add properties X:Matrix(100x20)[tall] y:Matrix(100x1)
add rule-set factorization algebraic inverse orthogonality
parse Mul(Mul(Inv(Mul(Tr(X),X)),Tr(X)),y)
parse Mul(Tr(X),y)
rewrite 5
show state
extract 6
```

Run it with:

```bash
./run_cli.sh
```

## Running tests

```bash
# Run all unit tests (no coverage)
cmake -S . -B build
cmake --build build --target unit_tests
./build/tests/unit_tests

# Or use the helper script (also collects coverage with grcov)
./run_tests.sh

# Run a specific test filter
./run_tests.sh "EGraphTest.*"
```

## Project structure

```
src/
  core/        # EGraph, ENode, EClass, UnionFind, Expression
  rewrite/     # Pattern matching, Rewriter
  extract/     # Cost model, Extractor
  prune/       # Pruner
  analysis/    # PropertyTable, Analysis
  io/          # DOT / image visualization
  common/      # Shared utilities, built-in rewrite sets
  main.cpp     # Interactive CLI entry point
tests/
  unit/        # Unit tests (GoogleTest)
  integration/ # Integration tests
  support/     # Test helpers
```
