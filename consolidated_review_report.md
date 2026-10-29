# Consolidated Review Report: Optional Pattern Grammar, Symbolic Shape Deduction & Unified BLAS Lowering

**Target Document:** [`docs/designs/optional_patterns_and_unified_kernel_lowering.md`](file:///Users/changguo/Projects/egraph-cpp/docs/designs/optional_patterns_and_unified_kernel_lowering.md)  
**Execution Mode:** Uninterrupted Full Flow (Stages 5, 6, 7, 8, 9)  
**Date:** 2026-10-28  
**Synthesizer:** Antigravity / EGM intermediate Swarm Compiler  

---

## Executive Summary

The uninterrupted multi-lens audit of the design document has concluded across all 5 specialized review stages:
1. **Stage 5 (Initial Zoom Out)**: [`zoom_out_report.md`](file:///Users/changguo/Projects/egraph-cpp/zoom_out_report.md)
2. **Stage 6 (Comprehension Test)**: [`comprehension_report.md`](file:///Users/changguo/Projects/egraph-cpp/comprehension_report.md)
3. **Stage 7 (Critic Review)**: [`critic_report.md`](file:///Users/changguo/Projects/egraph-cpp/critic_report.md)
4. **Stage 8 (Intermediate Zoom Out)**: [`intermediate_zoom_out_report.md`](file:///Users/changguo/Projects/egraph-cpp/intermediate_zoom_out_report.md)
5. **Stage 9 (Implementation Readiness Test)**: [`implementation_readiness_report.md`](file:///Users/changguo/Projects/egraph-cpp/implementation_readiness_report.md)

### Overall Verdict: 🔴 REVISION REQUIRED (4 Consolidated Blockers, 4 Majors, 3 Nits)

While the goal of collapsing 20+ fragmented BLAS rewrite rules into canonical parameterized kernels (`Op::Gemm`, `Op::Trsm`, etc.) is high-value, the design accumulated severe architectural flaws, scope creep, and broken C++ abstractions that prevent immediate implementation:
* **Scope Over-Reach (YAGNI)**: The design coupled BLAS lowering with an unrelated frontend DSL shape inference overhaul (Union-Find dimension deduction).
* **Leaky Abstraction for Absent Bias**: Synthesizing procedural zero-matrix appliers for omitted bias terms is an anti-pattern. CBLAS natively represents absent bias via $\beta = 0.0$ without allocating artificial zero matrices.
* **Broken Pattern Grammar & Expander**: The formal EBNF grammar rejects headline pattern syntax (`[Tr](?a)`, `[?alpha *]`, `[+ ?c]`), and variable defaulting gaps cause crashes on absent scalar factors.
* **Type Collapse in `ScalarOperand` & `EvalFn`**: `ScalarOperand` is hardcoded as `double`, converting `ScalarExpr` to `NaN`, while `EvalFn` accepts no `DataBindings`, breaking runtime evaluation of variable scalar factors.
* **Omission of 7 Essential Build/Analysis Files**: The implementation plan omitted `CMakeLists.txt`, `analysis.h/.cpp`, `op_costs.h`, `evaluator.h`, and `cli.cpp`.

---

## Traceability Matrix

| Consolidated Action Item | Source Stage & ID | Priority | Affected Scope | Status |
| :--- | :--- | :---: | :--- | :---: |
| **ACT-1: Scope Decoupling & Elimination of Synthetic Zero Matrices** | St. 5 #1, #2; St. 8 #1, #2; St. 9 #3, #6 | 🔴 Blocker | Sec. 2 (Lines 186–229), Sec. 3 (Lines 254–278) | Proposed Fix |
| **ACT-2: Pattern EBNF Grammar Reconstruction & Variable Defaulting** | St. 6 #1, #2; St. 7 #1, #2, #3 | 🔴 Blocker | Sec. 1 (Lines 158–170), Sec. 3 (Lines 231–278) | Proposed Fix |
| **ACT-3: Fix `ScalarOperand` Abstraction & `EvalFn` Signature** | St. 6 #3, #6; St. 7 #4; St. 8 #3; St. 9 #4, #5 | 🔴 Blocker | Sec. 5 (Lines 326–330), Sec. 7 (Lines 358–377), Targets 2, 8 | Proposed Fix |
| **ACT-4: Expand Implementation Plan to 21 Files & Fix CLI Fixtures** | St. 6 #5; St. 9 #1, #2, #6, #7 | 🔴 Blocker | Sec. 9 (Lines 408–554), Verification Plan | Proposed Fix |
| **ACT-5: Complete TRSM, TRMM, SYMM, GEMV Lowering Coverage** | St. 6 #7, #8; St. 7 #7, #8; St. 9 #8, #9 | 🟡 Major | Sec. 5 (Table), Sec. 9 (Target 10) | Proposed Fix |
| **ACT-6: Formalize `PropertyTag` Struct & Reconcile Name Taxonomy** | St. 5 #5; St. 6 #9, #11; St. 7 #15; St. 8 #4; St. 9 #10, #16 | 🟡 Major | Sec. 4 (Lines 284–309), Targets 5, 7 | Proposed Fix |
| **ACT-7: In-Place Buffer Mutation Safety for Conditional Bias** | St. 5 #6; St. 7 #10; St. 8 #5 | 🟡 Major | Sec. 5 (Table), Sec. 7 (Evaluator) | Proposed Fix |
| **ACT-8: Disambiguation Fallback for Untyped DSL Identifiers** | St. 7 #9; St. 8 #6; St. 9 #15 | 🟡 Major | Sec. 6 (Scalars), Parser Specification | Proposed Fix |
| **ACT-9: Style & Minor Fixes (BLAS Classification, Null Checks)** | St. 6 #10, #12; St. 7 #12, #13, #14 | 🟢 Nit | Lines 10, 260–267 | Proposed Fix |

---

## Consolidated Action Items & Technical Solutions

### 🔴 ACT-1: Decouple Frontend Shape Inference & Eliminate Synthetic Zero Matrices
* **Root Issues**:
  1. Conflating DSL frontend shape deduction with E-Graph BLAS lowering bloated the PR to 14 incomplete files (Stage 5 Blocker 1, Stage 8 Blocker 1).
  2. Synthesizing dynamic C++ zero-matrix appliers embeds procedural matrix knowledge into a generic rewrite engine, allocates artificial zero nodes during saturation, and violates CBLAS semantics (Stage 5 Blocker 2, Stage 8 Blocker 2).
* **Remediation**:
  1. **Excise Section 2 (Automatic Shape Deduction) from this design**. Keep Block 1 (`symbols`) explicit. Defer automatic shape inference to a dedicated frontend ergonomic proposal.
  2. **Eliminate synthetic zero-matrix appliers**. In CBLAS, absent bias is natively represented by setting scalar $\beta = 0.0$. In `Op::Gemm`, when `+ ?c` is absent, the expander binds an interned null sentinel or omits $C$, and sets scalar $\beta = 0.0$. In `eval_gemm`, when $\beta == 0.0$, the evaluator passes `beta = 0.0` directly to `cblas_dgemm` without allocating or reading a zero matrix.

### 🔴 ACT-2: Reconstruct Pattern EBNF Grammar & Fix Unbound Variable Defaulting
* **Root Issues**:
  1. EBNF grammar in Section 1 fails to parse `[Tr](?a)` (cannot call optional wrapper), `[?alpha *]` (trailing operator error), and `[+ ?c]` (leading operator error) (Stage 6 Blocker 1, Stage 7 Blocker 1).
  2. When optional terms are omitted, absent variables (`?alpha`, `?beta`) fail during `Pattern::instantiate` because matchers do not bind absent variables (Stage 7 Blocker 2, Blocker 3).
* **Remediation**:
  1. Update pattern EBNF productions to explicitly accommodate optional operators and affixes:
     ```ebnf
     pattern_factor      = optional_op_call
                         | optional_clause
                         | pattern_var
                         | identifier , "(" , [ pattern_list ] , ")"
                         | "(" , pattern_expr , ")"
                         | number_literal
                         | identifier ;
     optional_op_call    = "[" , identifier , "]" , "(" , pattern_list , ")" ;
     optional_clause     = "[" , ( "+" | "-" ) , pattern_expr , "]"
                         | "[" , pattern_expr , ( "*" | "/" ) , "]"
                         | "[" , pattern_expr , "]" ;
     pattern_term        = pattern_factor , { [ "*" | "/" ] , pattern_factor } ;
     ```
  2. In `expand_rewrites`, when generating concrete permutations, bake constant default literals directly into the RHS pattern string (e.g. rewriting `"Gemm(?a, ?b, ?c, ?alpha, ?beta, ?tr_a, ?tr_b)"` to `"Gemm(?a, ?b, ?c, 1.0, 0.0, 1, 0)"` for that specific branch). This guarantees $O(1)$ structural instantiation without dynamic applier crashes.

### 🔴 ACT-3: Correct `ScalarOperand` Type Abstraction & `EvalFn` Callback Signature
* **Root Issues**:
  1. `src/core/op_registry.h:44` defines `using ScalarOperand = double;`. In `init_operands`, `ScalarExpr` is converted to `NaN`, discarding the AST (Stage 6 Blocker 3, Stage 9 Blocker 4).
  2. `OpDescriptor::EvalFn` accepts no `DataBindings`, and `as_scalar(operands[3]).value_or(1.0)` fails to compile (Stage 7 Blocker 4, Stage 9 Blocker 5).
* **Remediation**:
  1. Redefine in `src/core/op_registry.h`:
     ```cpp
     using ScalarOperand = std::variant<double, ScalarExpr>;
     ```
  2. Update `init_operands` in `src/core/op_registry.cpp` to preserve `ScalarExpr` from atom.
  3. Update `EvalFn` in `src/core/op_registry.h`:
     ```cpp
     using EvalFn = std::function<void(
         std::string_view op_name, const std::vector<Operand> &operands,
         const std::vector<const MatrixNode *> &inputs, MatrixNode &output,
         const DataBindings &data_bindings)>;
     ```
  4. In `src/evaluate/evaluator.cpp`, update `dispatch_matrix_kernel` to pass `this->data_bindings`, and implement helper `double resolve_scalar(const Operand &op, const DataBindings &bindings, double default_val = 1.0)`.

### 🔴 ACT-4: Expand Implementation Plan to 21 Files & Fix CLI Runner
* **Root Issues**:
  1. The Detailed Implementation Plan omitted 7 files (`src/CMakeLists.txt`, `tests/CMakeLists.txt`, `src/analysis/analysis.h/.cpp`, `src/cost_functions/op_costs.h`, `src/evaluate/evaluator.h`, `src/cli.cpp`) (Stage 9 Blocker 1).
  2. Verification Plan specifies `egraph_cli run ...`, but `src/cli.cpp` ignores `argv` and has no `run` command (Stage 6 Blocker 5, Stage 9 Blocker 2).
* **Remediation**:
  1. Expand the implementation plan to 21 files as specified in the Stage 9 Remediation Matrix.
  2. Add `src/cli.cpp` modification to parse `run <filename>`, load the script via `parse_program`, and execute directives.
  3. Commit test fixture `tests/fixtures/ols_pipeline.eg` and update `tests/CMakeLists.txt` to build all new test targets.

### 🟡 ACT-5: Complete Lowering Rule Permutations for TRSM, TRMM, SYMM, GEMV
* **Root Issues**:
  1. `Op::Symm` omits the required `uplo` parameter (Stage 7 Major 8, Stage 9 Major 8).
  2. Specifying only 2 TRSM rules omits lower-triangular left solves, upper-triangular right solves, and transpose solves (Stage 6 Major 7, Stage 9 Major 9).
  3. GEMV and SYMM rules were omitted from Target File 10.
* **Remediation**:
  1. Update `Op::Symm` operand schema to 7 operands: `[A, B, C, alpha, beta, side, uplo]`.
  2. Specify the complete set of 8 rewrite rules covering all combinations of `Side` (Left/Right), `Uplo` (Upper/Lower), and `Trans` (NoTrans/Trans) for TRSM and TRMM.
  3. Add explicit rules for `Op::Gemv` and `Op::Symm`.

### 🟡 ACT-6: Formalize `PropertyTag` Structure & Reconcile Taxonomy
* **Root Issues**:
  `MatrixFlags` has 15 boolean fields (not a bitmask), tags `square`/`vector` are shape checks, and short names (`spd`, `upper`) diverge from descriptors in `property_table.h` (Stage 6 Major 9, Stage 8 Major 4).
* **Remediation**:
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
  Provide alias mappings so both canonical short names (`spd`, `upper`) and full descriptors (`positive_definite`, `upper_triangular`) are recognized.

### 🟡 ACT-7: In-Place Buffer Mutation Safety for Conditional Bias
* **Root Issues**: Setting `in_place_input = 2` on an absent or freshly generated buffer implies mutating constant nodes (Stage 7 Major 10, Stage 8 Major 5).
* **Remediation**:
  Document in `OpDescriptor` and `evaluator.cpp` that `in_place_input = 2` is strictly active only when an explicit accumulator matrix is supplied ($A \cdot B + C$ where $\beta \ne 0.0$). When $\beta == 0.0$, in-place buffer reuse is bypassed and a fresh output buffer is allocated.

### 🟡 ACT-8: Disambiguation Fallback for Untyped DSL Identifiers
* **Root Issues**: If identifiers are undeclared, parse-time scalar disambiguation cannot distinguish scalar variables from matrices (Stage 7 Major 9).
* **Remediation**:
  Document that in concrete DSL scripts, all identifiers default to `Matrix` unless declared as `Scalar` in the `symbols` block or bound to a scalar numeric literal (`2.0`).

### 🟢 ACT-9: Style & Minor Fixes
* Update BLAS classification to "Level 2 and Level 3 BLAS operations" (line 10).
* Add pointer null guards in property lookup utilities.
* Update `ScalarExpr::evaluate` to throw `std::runtime_error` on missing variables.

---

## Recommended Next Step

With all 5 review stages complete and synthesized, you can immediately apply these consolidated remediations by running:

```text
/egm:egm-resolve
```
