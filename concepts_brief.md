# Optional Operators in Pattern Grammar & Unified Parameterized Kernel Lowering

* **Author:** changguo & Antigravity
* **Date:** 2026-10-28
* **Status:** Finalized Ideation Brief (Input Context for /egm-draft)

## Overview

In `egraph-cpp`, BLAS lowering currently suffers from a combinatorial explosion of rewrite rules and fragmented kernel operators in `OpRegistry`. For example, General Matrix Multiply (GEMM) encompasses transpose permutations (`NN`, `TN`, `NT`, `TT`), optional additive bias terms (`+ C`), and scalar scaling factors ($\alpha \cdot A \cdot B + \beta \cdot C$), forcing 8 distinct hand-written rewrite rules and 4 distinct enum operations (`Gemm_NN`, `Gemm_TN`, `Gemm_NT`, `Gemm_TT`). Adding scalar factors causes a 32-rule explosion. Similar proliferation affects `Syrk`, `Trsm`, `Trmm`, and `Symm`.

This feature introduces a unified, declarative lowering architecture with automatic shape deduction:
1. **Simplified Optional Pattern Operators (`[Tr](?a)`)**: Automatic boolean flag binding (`?tr_a = 1/0`) without verbose aliases.
2. **Unified Property Tags (`{...}`)**: Disentangling properties from optional brackets by standardizing on `{...}` across both DSL symbol declarations (`A: Matrix{upper}`) and generic rewrite patterns (`?a{upper, spd}`).
3. **Automatic Symbolic Shape Deduction**: Equational shape inference via Disjoint-Set (Union-Find), making Block 1 `symbols` optional for simple pipelines and allowing extraction sizes to be specified by matrix name (`with sizes(X = 1000x20, W = 20x50)`).
4. **Closed-Form Leaf Scalar Representation**: Storing scalar expressions (including variables like `alpha`) as immutable `ScalarExpr` leaf E-nodes (`ENode({}, ScalarExpr("alpha"))`) in the E-Graph to protect against combinatorial state space explosion, evaluated dynamically at runtime via `DataBindings`.
5. **Unified Parameterized BLAS Kernels**: Collapsing fragmented kernel variants into single canonical operations (`Op::Gemm`, `Op::Syrk`, `Op::Trsm`, `Op::Trmm`, `Op::Symm`, `Op::Gemv`) with standardized operand signatures and preserved in-place buffer indices.
6. **Cartesian Product Rule Expander (`expand_rewrites`)**: Expanding pattern templates into standard $O(1)$ structural rewrites with dynamic applier synthesis for absent bias terms.

---

## Analysis

### 1. Pattern Grammar Syntax & Disentanglement
* **Optional Operators**: `[Op](?var)` automatically binds a boolean flag variable `?op_var`:
  - `[Tr](?a)` binds `?tr_a = 1` if `Tr` matched, or `?tr_a = 0` if absent.
  - `[+ ?c]` matches an optional additive term.
  - `[?alpha *]` matches an optional scalar multiplier.
* **Property Constraints**: Standardized on `{...}` to eliminate any collision with optional brackets:
  - In Rewrite Patterns: `?a{upper}`, `?a{symmetric, spd}`.
  - In DSL Declarations: `A: Matrix(100, 100){upper};` or `A: Matrix{spd};`.

### 2. Automatic Shape Deduction via Union-Find
* **Dimension Equivalence Classes**: Fresh dimension variables ($d_0, d_1, \dots$) are created for unannotated matrices and unified across operations ($A \times B \implies \text{cols}(A) = \text{rows}(B)$).
* **Optional Symbols Block**: If an expression pipeline contains no explicit properties or fixed shapes, Block 1 (`symbols`) can be omitted entirely.
* **Matrix-Based Size Binding**: At extraction time, sizes are bound by matrix name: `with sizes(X = 1000x20, W = 20x50)`.

### 3. Representation of Scalars with Variables in the E-Graph
* **Closed-Form Leaf E-Nodes**: A scalar variable like `alpha` is stored as an `ENode` with zero children and an `Atom(ScalarExpr(ScalarOp::Var, "alpha"))`.
* **Zero Combinatorial Explosion**: Scalar arithmetic is not expanded into individual algebraic E-nodes in the E-graph; it remains within `ScalarExpr` trees inside leaf atoms.
* **Runtime Evaluation**: Concrete execution in `evaluator.cpp` evaluates `scalar_expr.evaluate(data_bindings)` to pass `double` values directly to CBLAS.

### 4. Unified BLAS Kernel Layout in `OpRegistry` and `ENode`
Every unified BLAS kernel uses standardized child positions in `ENode.children` (stored as E-class IDs):
* **`Op::Gemm`** (7 operands): `[A, B, C, alpha, beta, transA, transB]` (`in_place_input = 2`)
* **`Op::Syrk`** (5 operands): `[A, C, alpha, beta, trans]` (`in_place_input = 1`)
* **`Op::Trsm`** (6 operands): `[A, B, alpha, side, uplo, trans]` (`in_place_input = 1`)
* **`Op::Trmm`** (6 operands): `[A, B, alpha, side, uplo, trans]` (`in_place_input = 1`)
* **`Op::Symm`** (6 operands): `[A, B, C, alpha, beta, side]` (`in_place_input = 2`)
* **`Op::Gemv`** (6 operands): `[A, x, y, alpha, beta, trans]` (`in_place_input = 2`)

### 5. Cartesian Rule Expander (`expand_rewrites`) & Dynamic Appliers
* Templates with $K$ optional clauses expand at startup into $2^K$ standard `Rewrite` objects ($K \le 5$).
* For branches where `?c` is absent, the expander generates a dynamic C++ `applier` lambda that calls `make_zero_of_shape(g, {rows, cols})` to instantiate the zero matrix before adding the `Op::Gemm` node.

---

## Goals vs. Non-Goals

### In-Scope Goals
* **G1: Concise Optional Pattern Syntax**: Support `[Op](?var)`, `[+ ?var]`, and `[?scalar *]` in pattern grammar.
* **G2: Unified Property Tagging**: Support `{tag1, tag2}` across patterns and DSL symbol declarations.
* **G3: Symbolic Shape Unification**: Implement Disjoint-Set shape deduction so `symbols` block is optional.
* **G4: Matrix-Based Size Binding**: Enable extraction size binding via `with sizes(MatrixName = Rows x Cols)`.
* **G5: First-Class Closed-Form Scalar Variables**: Store `ScalarExpr` variables in leaf E-nodes without algebraic E-graph expansion.
* **G6: Universal BLAS Parameterization**: Unify all Level 3 BLAS operations in `OpRegistry` with preserved in-place indices.
* **G7: Cartesian Rule Expander with Dynamic Appliers**: Implement `expand_rewrites` generating concrete rewrites and shape-derived zero matrix appliers.

### Out-of-Scope Non-Goals
* **NG1: No Dynamic Matcher Backtracking**: Saturation engine remains strictly structural and deterministic with $O(1)$ search.
* **NG2: No 1x1 Matrix Allocation for Scalars**: Scalars remain dedicated `ScalarExpr` structures.
* **NG3: No General Regex Patterns**: Only binary optionality `[...]` and property constraints `{...}` are supported.
* **NG4: No Backwards Compatibility Constraints**: Legacy enum kernels (`Gemm_NN`, `Trsm_LN`, etc.) and deprecated string hacks are deleted outright.
