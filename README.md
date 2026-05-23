# egraph-cpp

A C++ implementation of an [e-graph](https://egraphs-good.github.io/) (equality graph) data structure, specialized for optimizing **linear algebra and matrix expressions** via equality saturation.

## Overview

E-graphs compactly represent a large set of equivalent expressions. This project leverages e-graphs to discover algebraically simpler or computationally cheaper forms of matrix expressions by:
- Eliminating redundant inverses.
- Exploiting matrix properties (orthogonality, symmetry, etc.).
- Selecting optimal matrix factorizations (QR, LU, Cholesky).
- Converting matrix inversions into optimized solver calls (`Sol(A, B)`).

### Running the CLI

```bash
# Builds and runs the CLI with input.txt
python3 run_cli.py

# Runs the CLI with input_symbolic.txt
python3 run_cli.py 1
```

### Running Tests

```bash
# Builds and runs all unit and integration tests
python3 run_tests.py

# Run tests matching a specific pattern (gtest_filter)
python3 run_tests.py "Integration.*"
```

## Expression Syntax

- **Infix Operators**: `A * B` (multiply), `A + B` (add), `A - B` (subtract).
- **Functions**:
  - `Tr(A)`: Transpose
  - `Inv(A)`: Inverse
  - `Sol(A, B)`: Solve $AX = B$
  - `SolR(B, A)`: Solve $XA = B$
  - `QR(A)`, `LU(A)`, `LLt(A)`: Factorizations
  - `Get(tuple, index)`: Extract component from factorization (e.g., `Get(QR(A), 0)` for $Q$)
  - `Det(A)`, `Log(A)`: Determinant and Logarithm

## Matrix Properties

Properties enable condition-guarded rewrites (e.g., $(A^T A)^{-1}$ only exists if $A$ is tall and full rank):

```bash
add properties X:Matrix(100x20)[tall][positive_definite] y:Matrix(100x1)
```

**Available Flags**: `tall`, `wide`, `square`, `symmetric`, `positive_definite`, `orthogonal`, `orthonormal`, `singular`, `identity`, `zero`, `permutation`, `lower_triangular`, `upper_triangular`, `diagonal`.

## Rewrite Rule Sets

| Name | Description |
|------|-------------|
| `algebraic` | Associativity, identity elimination, transpose of products. |
| `inverse` | Inverse cancellation ($A A^{-1} = I$), inverse of products. |
| `orthogonality` | Exploits $Q^T Q = I$ for orthogonal matrices. |
| `factorization` | Inversion rules based on QR, LU, and Cholesky decompositions. |
| `solver` | Converts `Inv(A) * B` into `Sol(A, B)` and optimizes solver chains. |
| `zero-negation` | Zero absorption and subtraction simplification. |
| `complete` | Includes all of the above rule sets. |

## Example Session: Ordinary Least Squares (OLS)

Optimizing the estimator $(X^T X)^{-1} X^T y$:

```bash
# 1. Define properties
add properties X:Matrix(100x20)[tall][positive_definite] y:Matrix(100x1)

# 2. Enable rules
add rule-set complete

# 3. Parse expression
parse Inv(Tr(X) * X) * Tr(X) * y

# 4. Rewrite for 10 iterations
rewrite 10

# 5. Extract (index 0 is the OLS expression)
extract 0
```

## Extraction Heuristics

To navigate the massive search space of matrix optimizations, the engine relies on several heuristics that prioritize practical performance over exhaustive optimality.

Finding the globally optimal expression with shared sub-expressions is NP-hard. Our extractor uses a bounded search guided by the following heuristics:

- **Greedy Initialization**: The search is seeded with an initial tree search pass to cauculate the minimal possible cost and minimal possible size. This pass ignores sub-expression sharing but provides a fast upper bound .
- **Search Prioritization**: The engine makes decisions on the most expensive sub-expressions first (based on their minimal possible cost). 
- **Local Best-First**: Within each e-class, nodes are explored in ascending order of their estimated tree cost.
- **Pruning Strategy**: If the current partial solution cannot outperform the worst solution in the top-$K$ results even with the calculated minimal possible DAG cost, the search branch is pruned immediately.
- **Max Depth** : A maximum search depth prevents search from going too deep into the DAG, and pruning is applied earlier when current depth + minimal possible depth exceeds the maximum.

## To be implemented:

- **Deduce Properties**: For instance A*A' is symmetric and positive definite if A is full rank.

## Project Structure

- `src/core/`: E-graph implementation and expression types.
- `src/rewrite/`: Pattern matching and equality saturation engine.
- `src/extract/`: Cost-based extraction logic.
- `src/common/`: Parsers and built-in rewrite sets.
- `tests/`: Extensive unit and integration test suite.
