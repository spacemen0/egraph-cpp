# egraph-cpp Blueprint

## System Overview
`egraph-cpp` is a high-performance C++20 equality saturation and term rewriting framework specialized for linear algebra, symbolic tensor expressions, and BLAS/LAPACK kernel optimization. Built upon the E-Graph (Equivalence Graph) data structure, the system optimizes complex mathematical expressions by exploring equivalence classes of algebraically identical programs without committing to an eager ordering of rewrites.

The system features:
- **E-Graph Core**: Union-Find based congruence closure, equality saturation with memoization, and automated rebuilding.
- **Abstract Domain & Property Analysis**: Lattice-based matrix property analysis tracking shape dimensions, symmetry, triangularity, orthogonality, positive (semi-)definiteness, and rank.
- **Rule Engine & Scheduling**: Directional, bidirectional, conditional, and dynamic rule appliers supported by a backoff scheduler to mitigate combinatorial node explosion.
- **Cost-Driven Extraction**: Analytical FLOP-based cost modeling with both greedy tree extraction and global DAG search for optimal common subexpression reuse.
- **Hardware Lowering & Evaluation**: Automated lowering of symbolic linear algebra terms into optimized BLAS and LAPACK routines (dispatched via Apple Accelerate or OpenBLAS/LAPACKE).

---

## Subdirectories & System Boundaries

### Core Architecture & Implementation (`src/`)
- **`src/` Root**: Public high-level library interface [`src/api.h`](file:///Users/changguo/Projects/egraph-cpp/src/api.h) (`EGraphRunner::Context`) and interactive command-line interface [`src/cli.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/cli.cpp).
- **`src/core`**: Implements foundational E-Graph representations including `EGraph`, equivalence classes (`EClass`), equivalence nodes (`ENode`), disjoint-set forest (`UnionFind`), AST expressions (`Expression`), and the extensible operator registry (`OpRegistry`).
- **`src/analysis`**: Abstract interpretation engine. Implements `MatrixAnalysis` and `PropertyTable`, inferring algebraic properties (e.g., symmetry, positive definiteness, orthonormality) and enforcing mathematical property hierarchies.
- **`src/rewrite`**: Rewrite and equality saturation engine. Houses `Rewriter`, graph/tree `Matcher`, AST `Pattern` abstractions, dynamic rule dispatch, and match limit backoff tracking.
- **`src/rule_sets`**: Modular rewrite rule suites covering algebraic simplification, structural transformations, property discovery, expansion, condition guards, and hardware lowering. See [REWRITE_RULES.md](./REWRITE_RULES.md) for full formal specifications.
- **`src/extract`**: Extraction engine (`Extractor`). Extracts cost-optimal expressions from the saturated E-Graph using greedy tree algorithms or branch-and-bound DAG search under FLOP-based metrics.
- **`src/prune`**: E-Graph pruner (`Pruner`). Mitigates state explosion by removing unreachable classes and pruning higher-cost symbolic nodes when concrete hardware kernels are available.
- **`src/cost_functions`**: Analytical FLOP and computational cost models (`op_costs`) for BLAS/LAPACK kernels and symbolic operations across numeric and symbolic dimension bindings.
- **`src/evaluate`**: Concrete numerical evaluation runtime (`Evaluator`). Coordinates buffer management, in-place storage formatting (`StorageFormat`), and BLAS/LAPACK kernel execution.
- **`src/common`**: Shared foundational utilities: configuration management (`egraph_config.h`), expression parser (`parser.h`), ruleset aggregation (`rewrite_sets.h`), error definitions (`errors.h`), and dimension/binding helper functions (`utils.h`).
- **`src/io`**: Visualization utilities (`EGraphVisualization`) providing Graphviz DOT exports and PNG/SVG rendering.

### Applications & Verification
- **`examples`**: Real-world end-to-end optimization benchmarks including Ordinary Least Squares (`OLS`), Generalized Least Squares (`GLS`), `KalmanFilter`, `KalmanGain`, `StochasticNewton`, and `ImageRestoration`.
- **`tests`**: Comprehensive test suites (185+ test cases) partitioned into `tests/unit` (component verification), `tests/integration` (end-to-end rewrite & extraction pipelines), and `tests/support` (testing harnesses).
- **`scripts`**: Developer tooling including test runners (`run_tests.py`), interactive CLI launcher (`run_cli.py`), and test data generators (`generate_example_data.py`).

---

## Global Invariants
- **Congruence & Cleanliness**: An `EGraph` instance must be restored to canonical form via `rebuild()` after any union operations or new node insertions before initiating pattern matching, extraction, or querying (`is_clean() == true`).
- **Monotonicity of Analysis**: Inferred abstract domain properties in `AnalysisData` monotonically accumulate information through `MatrixAnalysis::merge`. Derived properties cannot contradict declared properties; contradictory shapes or incompatible structures strictly throw `AnalysisError`.
- **Rewrite Semantic Soundness**: All rewrite rules in `rule_sets` must guarantee semantic equivalence over valid mathematical domains under their respective condition guards (e.g., conforming dimensions, non-singularity, or positive definiteness).
- **DAG Cycle Freedom**: Extraction algorithms (`Extractor::extract`, `Extractor::tree_extract`) must produce valid Directed Acyclic Graphs without cyclic class dependencies.
- **Buffer Non-Interference**: In-place execution optimizations within `Evaluator` must never overwrite memory buffers for nodes that are referenced as inputs by downstream operations or marked as preserved targets.

---

## Global Side Effects
- **Dynamic Memory Management**: High-throughput graph node and hash table allocations link against Microsoft `mimalloc` to avoid memory fragmentation during extensive equality saturation runs.
- **Hardware Acceleration Dispatch**: Numerical evaluation executes vendor-optimized BLAS/LAPACK libraries (Apple Accelerate on macOS; OpenBLAS and LAPACKE on Linux/Windows), utilizing multi-threaded vector instructions.
- **Filesystem & Rendering**: Graph visualization utilities (`to_dot_file`, `to_img`) create `.dot`, `.png`, or `.svg` files in the working directory when explicitly invoked.
- **Standard I/O Logging**: When `enable_logging` is enabled in `EGraphConfig`, detailed rewrite step progress, match statistics, and execution plans are streamed to standard output.

---

## System Verification
Build and execute the full test suite using CMake/CTest or the provided runner script:

```bash
# Run tests using CTest
ctest --test-dir build --output-on-failure

# Alternatively, run via project test script
python3 scripts/run_tests.py
```
