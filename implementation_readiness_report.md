# Implementation Readiness Test Report: Optional Pattern Grammar, Symbolic Shape Deduction & Unified BLAS Lowering

**Target Document:** [`/Users/changguo/Projects/egraph-cpp/docs/designs/optional_patterns_and_unified_kernel_lowering.md`](file:///Users/changguo/Projects/egraph-cpp/docs/designs/optional_patterns_and_unified_kernel_lowering.md) (`DetailedRange: lines 408–554`)  
**Reviewers:** `[egm-implementation-readiness, egm-codebase-details, egm-style]`  
**Standards:** `implementation-readiness-standard`  
**Target Output Path:** `/Users/changguo/Projects/egraph-cpp/implementation_readiness_report.md`  
**Context Integrated:** [`zoom_out_report.md`](file:///Users/changguo/Projects/egraph-cpp/zoom_out_report.md), [`comprehension_report.md`](file:///Users/changguo/Projects/egraph-cpp/comprehension_report.md), [`critic_report.md`](file:///Users/changguo/Projects/egraph-cpp/critic_report.md), [`intermediate_zoom_out_report.md`](file:///Users/changguo/Projects/egraph-cpp/intermediate_zoom_out_report.md)  

---

## Executive Summary

A Stage 9 **Implementation Readiness Audit** was executed on Section 9 ("Detailed Implementation Plan" and "Verification Plan", lines 408–554) of the proposed design document. The audit evaluated whether an engineer could execute the plan strictly as written without ambiguity, without inventing missing types, without encountering broken builds, and without making unguided architectural choices.

### Audit Verdict: 🔴 NOT IMPLEMENTATION READY (7 Blockers, 6 Majors, 3 Nits)

The Detailed Implementation Plan provides high-level narrative sketches of intent, but fails the implementation readiness test across multiple technical axes:
1. **Critical Omission of 7 Essential Files**: The plan lists 14 target files, but omits core infrastructure required to compile and run the project: `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `src/analysis/analysis.h`, `src/analysis/analysis.cpp`, `src/cost_functions/op_costs.h`, `src/evaluate/evaluator.h`, and `src/cli.cpp`.
2. **Unexecutable Verification Plan & Phantom Fixtures**: The primary verification command (`./build/bin/egraph_cli run tests/fixtures/ols_auto_shape.eg`) cannot execute: `src/cli.cpp` ignores `argc`/`argv`, implements no `run` command or file loader, and `tests/fixtures/ols_auto_shape.eg` does not exist on disk. Furthermore, newly created tests will never run because `tests/CMakeLists.txt` is not updated.
3. **Severe Type Abstraction Collapse**: In `src/core/op_registry.h:44`, `using ScalarOperand = double;`. In `op_registry.cpp:106–108`, `init_operands` converts any `ScalarExpr` to `quiet_NaN()`. The plan completely forgets to update `ScalarOperand` to hold `ScalarExpr`, causing evaluator callbacks to receive `NaN` or fail to compile.
4. **Signature Incompatibility on `EvalFn`**: `OpDescriptor::EvalFn` accepts no `DataBindings`, and `Evaluator::dispatch_matrix_kernel` does not pass `data_bindings`, making runtime evaluation of variable scalar factors (`alpha`, `beta`) in `eval_gemm` impossible.
5. **Phantom Functions in Parser**: The plan instructs modifying `parse_program()` in `src/common/parser.h`, but `parse_program()` does not exist in the codebase; `parser.h` contains only single-expression helpers.
6. **Inversion of Stage 8 Steering**: The plan persists the procedural synthesis of zero-matrix appliers for absent bias, ignoring Stage 8 guidance that CBLAS natively handles absent bias via $\beta = 0.0$ without zero-matrix allocation.

---

## Prioritized Findings (Blockers -> Major -> Nits)

### 1. Critical Omission of 7 Essential Build and Header Files - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-implementation-readiness` / `egm-codebase-details`
* **Affected Scope:** Lines 410–530 (`Target Files to Create / Modify`)
* **Discrepancy Description**:  
  The implementation plan omits 7 essential codebase files that MUST be modified for the project to compile, link, and execute:
  1. [`src/CMakeLists.txt`](file:///Users/changguo/Projects/egraph-cpp/src/CMakeLists.txt): New source `src/analysis/shape_inference.cpp` will not be compiled into `egraph_core_lib` without adding it to `add_library(egraph_core_lib STATIC ...)`.
  2. [`tests/CMakeLists.txt`](file:///Users/changguo/Projects/egraph-cpp/tests/CMakeLists.txt): New unit test sources (`shape_inference_test.cpp`, `pattern_expansion_test.cpp`, `lowering_test.cpp`) will never be built by `unit_tests` without adding them to `add_executable(unit_tests ...)`.
  3. [`src/analysis/analysis.h`](file:///Users/changguo/Projects/egraph-cpp/src/analysis/analysis.h) & [`src/analysis/analysis.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/analysis/analysis.cpp): Deleting 18 legacy ops (`Gemm_NN`, `Trsm_LN`, etc.) invalidates 18 declared `analyze_*` functions (`analyze_gemm_nn`, `analyze_gemm_tn`, etc.). New prototypes (`analyze_gemm`, `analyze_syrk`, `analyze_trsm`, `analyze_trmm`, `analyze_symm`, `analyze_gemv`) must be declared and implemented.
  4. [`src/cost_functions/op_costs.h`](file:///Users/changguo/Projects/egraph-cpp/src/cost_functions/op_costs.h): Omits function prototype updates consolidating cost calculations (`compute_gemm_cost`, `compute_syrk_cost`, etc.).
  5. [`src/evaluate/evaluator.h`](file:///Users/changguo/Projects/egraph-cpp/src/evaluate/evaluator.h): Omits declarations for consolidated evaluation functions (`eval_gemm`, `eval_syrk`, `eval_trsm`, `eval_trmm`, `eval_symm`, `eval_gemv`) and the updated callback parameter schema.
  6. [`src/cli.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/cli.cpp): Verification Plan step 3 invokes `egraph_cli run <file>`, but `cli.cpp` has no CLI argument handling.
  7. [`tests/fixtures/ols_auto_shape.eg`](file:///Users/changguo/Projects/egraph-cpp/tests/fixtures/ols_auto_shape.eg): The fixture required for CLI verification does not exist on disk.
* **Rationale & Recommendation**:  
  An implementer executing the plan will experience compilation failures or un-executed tests. Add all 7 missing files to the `Target Files to Create / Modify` catalog with explicit file-level diff instructions.

---

### 2. Unexecutable Verification Plan & Non-Existent CLI Subcommand - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-implementation-readiness`
* **Affected Scope:** Lines 535–554 (`Verification Plan`) vs [`src/cli.cpp:403–434`](file:///Users/changguo/Projects/egraph-cpp/src/cli.cpp#L403-L434)
* **Discrepancy Description**:  
  Line 547 defines the end-to-end integration test as:
  ```bash
  ./build/bin/egraph_cli run tests/fixtures/ols_auto_shape.eg
  ```
  However:
  1. In `src/cli.cpp:404–405`, `main` explicitly discards all command-line arguments:
     ```cpp
     int main(int argc, char **argv) noexcept {
         (void)argc;
         (void)argv;
         ...
     ```
  2. `cli.cpp` implements only an interactive line-by-line REPL (`SessionState`) over `std::cin`. It has no `run` command, no file opening logic, and no batch script execution parser.
  3. The fixture directory `tests/fixtures/` does not contain `ols_auto_shape.eg`.
  4. Line 538 builds `unit_tests`, but because `tests/CMakeLists.txt` is not modified, the new tests are never discovered or executed by `ctest`.
* **Rationale & Recommendation**:  
  Add `src/cli.cpp` modification to Target Files: implement CLI argument parsing for `run <filename>`, load script files into the parser, and commit the fixture `tests/fixtures/ols_auto_shape.eg`. Update `tests/CMakeLists.txt` to register all test files.

---

### 3. Undefined Runtime C++ Lambda Applier Generation from Strings - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-implementation-readiness` / `egm-codebase-details`
* **Affected Scope:** Line 469 (`Target File 6`) vs Lines 254–278 (`Section 3`)
* **Discrepancy Description**:  
  Line 469 states:
  > *"For branches without `?c`, synthesize dynamic C++ appliers that call `make_zero_of_shape` to construct shape-derived zero matrix nodes."*
  
  In C++, you cannot compile or synthesize a C++ lambda at runtime from a pattern string like `"Gemm(?a, ?b, ?c, ...)"`.  
  1. The plan specifies no function signature or return type for `expand_rewrites`.
  2. The plan provides no AST or interpreter for dynamically constructing e-nodes when RHS variables are absent.
  3. In `Substitution`, unbound variables (`?tr_a`, `?alpha`) are NOT present in the map; querying `s.count("tr_a")` always returns `0`.
  4. Stage 8 Intermediate Zoom Out already determined that synthesizing zero matrices is an anti-pattern. CBLAS represents absent bias by passing scalar $\beta = 0.0$ to `cblas_dgemm`. Pre-allocating zero matrices pollutes equality saturation and breaks in-place buffer annotations.
* **Rationale & Recommendation**:  
  Do not synthesize zero-matrix appliers. For branches without bias, expand RHS to `"Gemm(?a, ?b, ?zero_sentinel, ?alpha, 0.0, ?tr_a, ?tr_b)"` or generate concrete `Rewrite` rules via a deterministic C++ helper function in `lowering.h`. In `eval_gemm`, pass `beta = 0.0` directly to `cblas_dgemm`.

---

### 4. Type Abstraction Collapse: Dropped `ScalarExpr` in `ScalarOperand` & `init_operands` - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-codebase-details` / `egm-implementation-readiness`
* **Affected Scope:** Lines 421–434 (`Target File 2`), Line 485 (`Target File 8`), and [`src/core/op_registry.h:44`](file:///Users/changguo/Projects/egraph-cpp/src/core/op_registry.h#L44), [`src/core/op_registry.cpp:106–108`](file:///Users/changguo/Projects/egraph-cpp/src/core/op_registry.cpp#L106-L108)
* **Discrepancy Description**:  
  Target File 8 states: *"In `eval_gemm`, evaluate symbolic `ScalarExpr` operands against `data_bindings` via `scalar_expr.evaluate(data_bindings)` before passing `double` to `cblas_dgemm`."*  
  This is completely broken by the codebase's existing type definitions:
  1. In `src/core/op_registry.h:44`:
     ```cpp
     using ScalarOperand = double;
     ```
  2. In `src/core/op_registry.cpp:106–108`:
     ```cpp
     if (is_symbolic_scalar) {
         operands.emplace_back(std::numeric_limits<double>::quiet_NaN());
         continue;
     }
     ```
  3. `ScalarOperand` cannot hold a `ScalarExpr`. `init_operands` discards the expression tree and replaces it with `NaN`.
  4. Target File 2 modifies `op_registry.h` and `op_registry.cpp`, but omits redefining `ScalarOperand` and omits updating `init_operands` to preserve `ScalarExpr`.
* **Rationale & Recommendation**:  
  In `src/core/op_registry.h`, update `ScalarOperand`:
  ```cpp
  using ScalarOperand = std::variant<double, ScalarExpr>;
  ```
  In `src/core/op_registry.cpp` (`init_operands`), extract `std::get<ScalarExpr>(cnode->get_atom())` and push it into `operands`. Provide helper `double resolve_scalar(const Operand &op, const DataBindings &bindings, double default_val = 1.0)`.

---

### 5. Signature Incompatibility on Kernel Evaluator Callback (`EvalFn`) - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-codebase-details` / `egm-implementation-readiness`
* **Affected Scope:** Lines 421–434 (`Target File 2`), Lines 479–486 (`Target File 8`), and [`src/core/op_registry.h:62–64`](file:///Users/changguo/Projects/egraph-cpp/src/core/op_registry.h#L62-L64), [`src/evaluate/evaluator.cpp:243`](file:///Users/changguo/Projects/egraph-cpp/src/evaluate/evaluator.cpp#L243)
* **Discrepancy Description**:  
  In `src/core/op_registry.h:62–64`:
  ```cpp
  using EvalFn = std::function<void(
      std::string_view op_name, const std::vector<Operand> &operands,
      const std::vector<const MatrixNode *> &inputs, MatrixNode &output)>;
  ```
  In `src/evaluate/evaluator.cpp:243`:
  ```cpp
  desc.eval(op_name, operands, inputs, output);
  ```
  `desc.eval` does NOT receive `const DataBindings &data_bindings`. If `eval_gemm` requires evaluating symbolic scalars against `data_bindings`, it cannot compile against `EvalFn`, nor can it access `Evaluator::data_bindings`.
* **Rationale & Recommendation**:  
  Update `EvalFn` in `src/core/op_registry.h` to accept `const DataBindings &data_bindings`:
  ```cpp
  using EvalFn = std::function<void(
      std::string_view op_name, const std::vector<Operand> &operands,
      const std::vector<const MatrixNode *> &inputs, MatrixNode &output,
      const DataBindings &data_bindings)>;
  ```
  Update line 243 of `src/evaluate/evaluator.cpp` to pass `this->data_bindings`.

---

### 6. Phantom Function `parse_program()` in `src/common/parser.h` - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-codebase-details` / `egm-implementation-readiness`
* **Affected Scope:** Lines 449–450 (`Target File 4`) vs [`src/common/parser.h:1–18`](file:///Users/changguo/Projects/egraph-cpp/src/common/parser.h#L1-L18)
* **Discrepancy Description**:  
  Target File 4 states:
  > *"Make `symbols` block optional in `parse_program()`. Integrate `ShapeInferenceContext` into expression parsing to infer dimensions when `PropertyTable` declarations are absent."*
  
  `src/common/parser.h` currently contains only two function declarations:
  ```cpp
  ParsedAtom parse_expression(std::string_view s);
  ScalarExpr parse_scalar(std::string_view s);
  ```
  `parse_program()` DOES NOT EXIST. There is no DSL program parser, no `ProgramAST` struct, and no multi-statement parser in `src/common/parser.h` or `parser.cpp`.
* **Rationale & Recommendation**:  
  Target File 4 must explicitly specify the AST structures and function signatures for program parsing:
  ```cpp
  struct ProgramAST {
      std::optional<PropertyTable> property_table;
      std::vector<std::pair<std::string, Expression>> operations;
      SizeBindings size_bindings;
  };
  ProgramAST parse_program(std::string_view source);
  ```
  Alternatively, adhere to Stage 8 Intermediate Zoom Out recommendations: excise DSL shape deduction and program parsing from this PR, keeping `symbols` blocks explicit.

---

### 7. Inaccurate File Status: `tests/unit/lowering_test.cpp` Listed as `(Modify)` - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-implementation-readiness`
* **Affected Scope:** Line 520 (`Target File 13`)
* **Discrepancy Description**:  
  Target File 13 states:
  > *"#### 13. `tests/unit/lowering_test.cpp` (Modify)"*
  
  Checking the filesystem: `tests/unit/lowering_test.cpp` DOES NOT EXIST on disk. It must be created from scratch. Listing it as `(Modify)` misleads implementers, automated agents, and git diff tooling into expecting an existing baseline.
* **Rationale & Recommendation**:  
  Change heading to `#### 13. tests/unit/lowering_test.cpp (Create)`. Specify the exact test fixtures, test cases (`TEST(LoweringTest, GemmUnifiedLowering)`), and expected graph assertions.

---

### 8. Missing `uplo` Flag in `Op::Symm` Operand Schema (BLAS Crash) - Severity: 🟡 Major
* **Reviewer Lens:** `egm-codebase-details` / `egm-implementation-readiness`
* **Affected Scope:** Line 432 (`Target File 2: Proposed Changes`) vs [`src/evaluate/evaluator.cpp:348–362`](file:///Users/changguo/Projects/egraph-cpp/src/evaluate/evaluator.cpp#L348-L362)
* **Discrepancy Description**:  
  Target File 2 defines the operand schema for `Op::Symm`:
  > *"`Op::Symm`: `[A, B, C, alpha, beta, side]` with `in_place_input = 2` (`C`)."*
  
  In BLAS (`cblas_dsymm`), symmetric matrix multiplication strictly requires both `side` (Left/Right) and `uplo` (Upper/Lower), because symmetric matrices are stored as upper or lower triangular matrices. In `evaluator.cpp:360`, `cblas_dsymm` takes `side` and `uplo_cblas`. Omitting `uplo` leaves the kernel unable to know which triangular half of $A$ contains valid data.
* **Rationale & Recommendation**:  
  Update `Op::Symm` child layout to 7 operands:
  `[A, B, C, alpha, beta, side, uplo]` with `in_place_input = 2`.

---

### 9. Incomplete Lowering Ruleset Coverage & Omission of GEMV and SYMM - Severity: 🟡 Major
* **Reviewer Lens:** `egm-implementation-readiness`
* **Affected Scope:** Lines 494–502 (`Target File 10`) vs [`src/rule_sets/lowering.h:15–60`](file:///Users/changguo/Projects/egraph-cpp/src/rule_sets/lowering.h#L15-L60)
* **Discrepancy Description**:  
  1. Target File 2 deletes `Gemv_N`, `Gemv_T`, `Symm_L`, and `Symm_R`.
  2. Target File 10 specifies rewrite rule replacements for GEMM, SYRK, TRSM, and TRMM, but completely omits GEMV and SYMM.
  3. Retaining existing GEMV/SYMM rules in `src/rule_sets/lowering.h` will throw `InvalidOperationError("Operation not found: Gemv_N")` at static initialization time.
  4. TRSM and TRMM are specified with only 2 rules (`trsm_left` and `trsm_right`), leaving lower-triangular left solves, upper-triangular right solves, and transpose solves unsupported (omitting 6 out of 8 permutations).
* **Rationale & Recommendation**:  
  Specify the exact replacement rules for `Op::Gemv` and `Op::Symm`, and specify all 8 permutations of `Side`, `Uplo`, and `Trans` for `Op::Trsm` and `Op::Trmm`.

---

### 10. Underspecified `struct PropertyTag` and Missing `MatrixFlags` Bitmask - Severity: 🟡 Major
* **Reviewer Lens:** `egm-codebase-details`
* **Affected Scope:** Lines 459–460 (`Target File 5`), Lines 476–478 (`Target File 7`), and [`src/analysis/property_table.h:15–37`](file:///Users/changguo/Projects/egraph-cpp/src/analysis/property_table.h#L15-L37)
* **Discrepancy Description**:  
  Line 477 states: *"Validate flags (`is_upper_triangular`, `is_symmetric`, etc.) using bitwise operations without intermediate string allocations."*  
  However:
  1. `MatrixFlags` in `src/analysis/property_table.h` consists of 15 boolean fields; it has no bitmask integer representation.
  2. Tags `square` and `vector` are dynamic shape equality checks (`shape.first == shape.second`, `shape.second == 1`), which cannot be tested via a flag bitmask.
  3. The plan gives no struct definition for `PropertyTag` or signature for `tag.test(prop)`.
* **Rationale & Recommendation**:  
  Define `struct PropertyTag` in `src/rewrite/pattern.h`:
  ```cpp
  struct PropertyTag {
      enum class Kind { FlagBit, IsSquare, IsVector };
      Kind kind = Kind::FlagBit;
      bool MatrixFlags::*member = nullptr;
      std::string name;
      bool test(const MatrixProperty &prop) const;
  };
  ```

---

### 11. Underspecified `lowering_set` Vector Construction in `lowering.h` - Severity: 🟡 Major
* **Reviewer Lens:** `egm-codebase-details` / `egm-implementation-readiness`
* **Affected Scope:** Lines 494–502 (`Target File 10`) vs [`src/rule_sets/lowering.h:223–265`](file:///Users/changguo/Projects/egraph-cpp/src/rule_sets/lowering.h#L223-L265)
* **Discrepancy Description**:  
  `src/rule_sets/lowering.h` exports rules via a static vector: `static const std::vector<Rewrite> lowering_set = { ... };`.  
  `expand_rewrites(...)` returns `std::vector<Rewrite>`. In C++, a vector cannot be placed as a single element in a braced initializer list for `vector<Rewrite>`.  
  The plan gives no executable code showing how `lowering_set` is constructed from both single `make_rewrite` instances and `expand_rewrites` vector outputs.
* **Rationale & Recommendation**:  
  Provide the factory function signature:
  ```cpp
  std::vector<Rewrite> get_lowering_ruleset();
  ```
  or specify a helper lambda concatenating vectors into `static const std::vector<Rewrite> lowering_set`.

---

### 12. Broken `ScalarExpr::to_string()` and Missing Constructor Overloads - Severity: 🟡 Major
* **Reviewer Lens:** `egm-codebase-details`
* **Affected Scope:** Lines 412–420 (`Target File 1`) vs [`src/core/basic_types.h:68–93`](file:///Users/changguo/Projects/egraph-cpp/src/core/basic_types.h#L68-L93)
* **Discrepancy Description**:  
  In `src/core/basic_types.h:79`, `ScalarExpr::to_string()` contains:
  ```cpp
  case ScalarOp::Var:
      return std::string(1, var);
  ```
  When `char var = '\0';` is replaced with `std::string var;`, `std::string(1, var)` will fail to compile.  
  Furthermore, existing tests construct `ScalarExpr('c')`. Omitting the `char` constructor or omitting `ScalarExpr(const char*)` breaks existing test suites.
* **Rationale & Recommendation**:  
  Target File 1 must include:
  ```cpp
  ScalarExpr(char c) : op(ScalarOp::Var), var(1, c) {}
  ScalarExpr(std::string s) : op(ScalarOp::Var), var(std::move(s)) {}
  ScalarExpr(const char *s) : op(ScalarOp::Var), var(s) {}
  ```
  and update `to_string()` to `return var;`.

---

### 13. Missing Identifier-to-Dimension Mapping in `ShapeInferenceContext` - Severity: 🟡 Major
* **Reviewer Lens:** `egm-codebase-details`
* **Affected Scope:** Lines 435–443 (`Target File 3`) vs Lines 188–210 (`Section 2`)
* **Discrepancy Description**:  
  Target File 3 specifies:
  > *"Implement matrix-based size binding resolution (`with sizes(X = 1000x20, W = 20x50)`)."*
  
  However, `ShapeInferenceContext` has no member tracking which matrix identifier corresponds to which dimension class IDs. When `resolve_bindings` is called with `"X"`, it cannot look up the row and column equivalence classes for `"X"`.
* **Rationale & Recommendation**:  
  Add `std::unordered_map<std::string, std::pair<int, int>> matrix_dims;` and `void register_matrix(std::string_view name, int row_dim, int col_dim);` to `ShapeInferenceContext`.

---

### 14. Redundant Disjoint-Set Wheel Reinvention - Severity: 🟢 Nit
* **Reviewer Lens:** `egm-codebase-details` / `egm-style`
* **Affected Scope:** Line 439 (`Target File 3`) vs [`src/core/union_find.h`](file:///Users/changguo/Projects/egraph-cpp/src/core/union_find.h)
* **Discrepancy Description**:  
  Line 439 specifies implementing `ShapeInferenceContext` with custom Disjoint-Set Union-Find data structures, ignoring the tested `egraph::UnionFind` class in `src/core/union_find.h`.
* **Rationale & Recommendation**:  
  Mandate reuse of `egraph::UnionFind` inside `ShapeInferenceContext`.

---

### 15. Missing Exception Policy on Unbound Variable Evaluation - Severity: 🟢 Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** Line 419 (`Target File 1`) vs [`src/core/basic_types.h:52`](file:///Users/changguo/Projects/egraph-cpp/src/core/basic_types.h#L52)
* **Discrepancy Description**:  
  Line 52 of `basic_types.h` silently returns `0.0` when a variable is missing from `data_bindings`. In mathematical optimizations, a missing step size parameter defaulting to `0.0` causes silent algorithm divergence.
* **Rationale & Recommendation**:  
  Specify that `ScalarExpr::evaluate(data_bindings)` throws `std::runtime_error("Unbound scalar variable: " + var)` when not found in `data_bindings`.

---

### 16. Inconsistent Property Tag Names vs Established Flag Descriptors - Severity: 🟢 Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** Line 477 (`Target File 7`) vs [`src/analysis/property_table.h:44–60`](file:///Users/changguo/Projects/egraph-cpp/src/analysis/property_table.h#L44-L60)
* **Discrepancy Description**:  
  Section 4 introduces short property tag names (`spd`, `fullrank`, `nonsingular`, `upper`, `lower`) that diverge from the descriptors in `property_table.h` (`positive_definite`, `full_rank`, `non_singular`, `upper_triangular`, `lower_triangular`).
* **Rationale & Recommendation**:  
  Explicitly specify alias mappings in `PropertyTag` construction so that both canonical short names and backward-compatible long names are accepted.

---

## Actionable Remediation Matrix: Complete Target File Specification

The implementation plan must be expanded from 14 incomplete items to **21 exhaustive, exact file specifications**:

| # | File Path | Action | Exact Required Changes & Signatures |
| :--- | :--- | :---: | :--- |
| **1** | [`src/core/basic_types.h`](file:///Users/changguo/Projects/egraph-cpp/src/core/basic_types.h) | Modify | `std::string var;`<br>`ScalarExpr(char c) : op(ScalarOp::Var), var(1, c) {}`<br>`ScalarExpr(std::string s) : op(ScalarOp::Var), var(std::move(s)) {}`<br>`ScalarExpr(const char *s) : op(ScalarOp::Var), var(s) {}`<br>Update `to_string()`: `case ScalarOp::Var: return var;`<br>Update `evaluate()`: throw if `var` missing from `data_bindings`. |
| **2** | [`src/core/op_registry.h`](file:///Users/changguo/Projects/egraph-cpp/src/core/op_registry.h) | Modify | Redefine `using ScalarOperand = std::variant<double, ScalarExpr>;`<br>Update `using EvalFn = std::function<void(std::string_view, const std::vector<Operand>&, const std::vector<const MatrixNode*>&, MatrixNode&, const DataBindings&)>;`<br>Delete 18 legacy enums (`Gemm_NN`...); register `Op::Gemm`, `Op::Syrk`, `Op::Trsm`, `Op::Trmm`, `Op::Symm`, `Op::Gemv`.<br>Set `Op::Symm` operands to `[A, B, C, alpha, beta, side, uplo]`. |
| **3** | [`src/core/op_registry.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/core/op_registry.cpp) | Modify | In `init_operands`, extract `ScalarExpr` from atom instead of inserting `quiet_NaN()`.<br>Update `init_builtin_ops` to register canonical ops with unified `AnalyzeFn`, `CostFn`, `EvalFn`. |
| **4** | [`src/analysis/analysis.h`](file:///Users/changguo/Projects/egraph-cpp/src/analysis/analysis.h) | Modify | **[OMITTED FILE]** Remove 18 legacy `analyze_*` declarations.<br>Declare `analyze_gemm`, `analyze_syrk`, `analyze_trsm`, `analyze_trmm`, `analyze_symm`, `analyze_gemv`. |
| **5** | [`src/analysis/analysis.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/analysis/analysis.cpp) | Modify | **[OMITTED FILE]** Implement shape & property propagation for `analyze_gemm` (derives $(M, N)$ from `transA`, `transB`), `analyze_syrk`, `analyze_trsm`, etc. |
| **6** | [`src/cost_functions/op_costs.h`](file:///Users/changguo/Projects/egraph-cpp/src/cost_functions/op_costs.h) | Modify | **[OMITTED FILE]** Remove legacy group cost declarations.<br>Declare `compute_gemm_cost`, `compute_syrk_cost`, `compute_trsm_cost`, `compute_trmm_cost`, `compute_symm_cost`, `compute_gemv_cost`. |
| **7** | [`src/cost_functions/op_costs.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/cost_functions/op_costs.cpp) | Modify | Implement unified FLOP formulas reading `transA`, `transB`, `side` from `operands`. |
| **8** | [`src/evaluate/evaluator.h`](file:///Users/changguo/Projects/egraph-cpp/src/evaluate/evaluator.h) | Modify | **[OMITTED FILE]** Declare unified evaluation functions taking `const DataBindings &data_bindings`. |
| **9** | [`src/evaluate/evaluator.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/evaluate/evaluator.cpp) | Modify | Pass `this->data_bindings` in `dispatch_matrix_kernel`.<br>In `eval_gemm`, evaluate `ScalarExpr` for $\alpha, \beta$; pass $\beta = 0.0$ when bias is absent without allocating zero matrix. |
| **10** | [`src/rewrite/pattern.h`](file:///Users/changguo/Projects/egraph-cpp/src/rewrite/pattern.h) | Modify | Define `struct PropertyTag` with `kind`, `member`, `test(prop)`.<br>Add `std::vector<PropertyTag> property_tags` to `Pattern`. |
| **11** | [`src/rewrite/pattern.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/rewrite/pattern.cpp) | Modify | Parse property tags `?var{tag1, tag2}` into `property_tags`. |
| **12** | [`src/rewrite/matcher.h`](file:///Users/changguo/Projects/egraph-cpp/src/rewrite/matcher.h) | Modify | Declare `bool validate_property_constraints(Id eclass_id, const std::vector<PropertyTag> &tags) const;`. |
| **13** | [`src/rewrite/matcher.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/rewrite/matcher.cpp) | Modify | Implement $O(1)$ property checking against `MatrixProperty.flags`. |
| **14** | [`src/rule_sets/lowering.h`](file:///Users/changguo/Projects/egraph-cpp/src/rule_sets/lowering.h) | Modify | Replace legacy rules with unified rules using property constraints (`?a{upper_triangular}`); cover all 8 permutations of TRSM/TRMM; add GEMV and SYMM rules; define `std::vector<Rewrite> get_lowering_ruleset()`. |
| **15** | [`src/common/parser.h`](file:///Users/changguo/Projects/egraph-cpp/src/common/parser.h) | Modify | Declare `struct ProgramAST` and `ProgramAST parse_program(std::string_view source);`. |
| **16** | [`src/common/parser.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/common/parser.cpp) | Modify | Implement Pratt parser for property tags `{...}`, optional `symbols` block, and matrix size binding syntax. |
| **17** | [`src/cli.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/cli.cpp) | Modify | **[OMITTED FILE]** Parse command line arguments `run <filename>`, load script, execute pipeline. |
| **18** | [`src/CMakeLists.txt`](file:///Users/changguo/Projects/egraph-cpp/src/CMakeLists.txt) | Modify | **[OMITTED FILE]** Add `analysis/shape_inference.cpp` to `egraph_core_lib`. |
| **19** | [`tests/CMakeLists.txt`](file:///Users/changguo/Projects/egraph-cpp/tests/CMakeLists.txt) | Modify | **[OMITTED FILE]** Add `unit/shape_inference_test.cpp`, `unit/pattern_expansion_test.cpp`, and `unit/lowering_test.cpp` to `unit_tests`. |
| **20** | [`tests/fixtures/ols_auto_shape.eg`](file:///Users/changguo/Projects/egraph-cpp/tests/fixtures/ols_auto_shape.eg) | Create | **[OMITTED FILE]** Commit valid DSL fixture script for verification. |
| **21** | [`tests/unit/lowering_test.cpp`](file:///Users/changguo/Projects/egraph-cpp/tests/unit/lowering_test.cpp) | Create | **[CORRECTED STATUS]** Create unit tests verifying unified `Op::Gemm`, `Op::Trsm`, and property constraint matching. |

---

## Conclusion & Handoff

The implementation plan in its current state leaves critical architecture, file targets, and type bindings undefined. Persisting this audit report to `/Users/changguo/Projects/egraph-cpp/implementation_readiness_report.md` provides the exact blueprint and file-by-file specification needed to remediate lines 408–554 before commencing code changes.
