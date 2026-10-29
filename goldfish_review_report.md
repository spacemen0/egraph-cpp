# Goldfish Annotated Review Report

**Target:** `docs/designs/optional_patterns_and_unified_kernel_lowering.md` (`lines 1–649`)  
**Status: resolved**

## Prioritized Findings & AI Suggestions

### 1. Incompatible EBNF Grammar for Pattern Templates [`egm-design`, `egm-structural`, `egm-comprehension`] - Severity: Blocker
* **Affected Scope:** [Lines 158–170](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L158-L170) (Section 1: Formal EBNF Grammar Specification)
* **Discrepancy Description:** The formal EBNF grammar fails to parse the canonical lowering patterns in CUJ 1 and CUJ 3:
  1. `[Tr](?a)`: `optional_wrapper` cannot be called as a function.
  2. `[?alpha *]`: Trailing operator `*` violates factor-term rules.
  3. `[+ ?c]`: Leading operator `+` violates expression-term rules.
* **Rationale:** A design specification cannot have an EBNF grammar that rejects its primary advertised syntax.
* ***AI Suggestion:*** PROPOSE FIX
* ***Justification:** Valid critique. Restructuring the pattern grammar to explicitly specify `optional_op_call` and `optional_clause` resolves all parsing errors while keeping syntax concise.
* ***Proposed Edit:***
  * Location: Section 1 EBNF (Lines 158–170)
  * Change: Replace pattern productions with:
    ```ebnf
    pattern_expr        = [ ( "+" | "-" ) ] , pattern_term , { ( "+" | "-" ) , pattern_term } ;
    pattern_term        = pattern_factor , { ( "*" | "/" ) , pattern_factor } ;
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
    pattern_var         = "?" , identifier , [ property_flags ] ;
    pattern_list        = pattern_expr , { "," , pattern_expr } ;
    ```

---

### 2. Unbound Variable Defaulting Gap in Expanded Pattern Templates [`egm-design`, `egm-comprehension`] - Severity: Blocker
* **Affected Scope:** [Lines 51–56](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L51-L56) (CUJ 1) & [Lines 233–278](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L233-L278) (Section 3)
* **Discrepancy Description:** When optional terms (like `[?alpha *]` or `[?beta *]`) are omitted on the LHS, variables `?alpha` and `?beta` are not matched. Attempting to instantiate the target RHS template `"Gemm(?a, ?b, ?c, ?alpha, ?beta, ?tr_a, ?tr_b)"` fails because `Substitution` does not contain defaulted values for absent variables.
* **Rationale:** Pattern instantiation in `egraph-cpp` does not support fallback lookups for missing variables.
* ***AI Suggestion:*** PROPOSE FIX
* ***Justification:** Valid critique. In `expand_rewrites`, when generating concrete rewrite rules for each Cartesian branch, constant default literals (`1.0` for absent `alpha`, `0.0` for absent `beta`) must be baked directly into the branch's RHS pattern template.
* ***Proposed Edit:***
  * Location: Section 3 (Lines 233–253)
  * Change: Specify that `expand_rewrites` bakes literal values into the RHS string for omitted optional variables:
    - If `[?alpha *]` was matched: RHS keeps `?alpha`. If omitted: RHS receives literal `1.0`.
    - If `[?beta *]` was matched: RHS keeps `?beta`. If omitted: RHS receives literal `1.0` (or `0.0` if `+ ?c` was omitted).
    - If `[Tr](?a)` was matched: RHS receives integer literal `1`. If bypassed: RHS receives integer literal `0`.

---

### 3. Flaw in Flag Binding Variable Resolution & Elimination of Synthetic Zero Matrices [`egm-design`, `egm-zoom-out`] - Severity: Blocker
* **Affected Scope:** [Lines 254–278](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L254-L278) (Section 3: Dynamic Applier Synthesis) & [Line 469](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L469)
* **Discrepancy Description:** 
  1. The dynamic applier inspects `s.count("tr_a")`, but `?tr_a` is never bound in the LHS pattern, causing `tr_a` to always evaluate to `0`.
  2. Synthesizing procedural zero matrices via `make_zero_of_shape` inside the rewrite engine embeds domain-specific BLAS knowledge, allocates artificial zero nodes during saturation, and breaks in-place buffer annotations.
* **Rationale:** In CBLAS, absent bias is natively represented by setting scalar $\beta = 0.0$. `cblas_dgemm` does not read $C$ when $\beta = 0.0$; it is strictly an output buffer.
* ***AI Suggestion:*** PROPOSE FIX
* ***Justification:** Valid critique. Eliminating synthetic zero-matrix appliers aligns the design with native CBLAS semantics, removes the need for procedural lambda synthesis in `expand_rewrites`, and prevents zero-node pollution in the E-Graph.
* ***Proposed Edit:***
  * Location: Section 3 (Lines 254–278) and Section 5 (Lines 317–330)
  * Change: Delete the dynamic zero-matrix applier. Specify that when `+ ?c` is omitted on the LHS:
    - The RHS template emits `Op::Gemm(?a, ?b, ?zero_sentinel, ?alpha, 0.0, ?tr_a, ?tr_b)`.
    - In `eval_gemm`, when `beta == 0.0`, the evaluator ignores operand `C`, allocates a fresh output buffer, and passes `beta = 0.0` directly to `cblas_dgemm`.

---

### 4. Type Abstraction Defect in `ScalarOperand` & `EvalFn` Signature [`egm-codebase-details`, `egm-comprehension`, `egm-design`] - Severity: Blocker
* **Affected Scope:** [Lines 326–330](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L326-L330) (Section 5), [Lines 358–377](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L358-L377) (Section 7), and [Lines 421–434](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L421-L434) (Target File 2)
* **Discrepancy Description:** 
  1. `src/core/op_registry.h:44` defines `using ScalarOperand = double;`. In `init_operands`, `ScalarExpr` is converted to `quiet_NaN()`.
  2. `OpDescriptor::EvalFn` accepts no `DataBindings`, and `as_scalar(operands[3]).value_or(1.0)` fails to compile.
* **Rationale:** Without updating `ScalarOperand` and `EvalFn`, evaluator callbacks cannot compile or dynamically evaluate symbolic scalar factors (`alpha`, `beta`).
* ***AI Suggestion:*** PROPOSE FIX
* ***Justification:** Valid critique. Updating the type definitions in `op_registry.h` ensures complete end-to-end type safety.
* ***Proposed Edit:***
  * Location: Section 5, Section 7, and Target File 2
  * Change:
    1. In `src/core/op_registry.h`:
       ```cpp
       using ScalarOperand = std::variant<double, ScalarExpr>;
       using EvalFn = std::function<void(
           std::string_view op_name, const std::vector<Operand> &operands,
           const std::vector<const MatrixNode *> &inputs, MatrixNode &output,
           const DataBindings &data_bindings)>;
       ```
    2. In `src/core/op_registry.cpp` (`init_operands`), extract `ScalarExpr` from atom instead of inserting `NaN`.
    3. In `src/evaluate/evaluator.cpp`, update `dispatch_matrix_kernel` to pass `this->data_bindings`, and implement helper `double resolve_scalar(const Operand &op, const DataBindings &bindings, double default_val = 1.0)`.

---

### 5. Monolithic Scope Conflation: Decouple Frontend Shape Inference [`egm-zoom-out`] - Severity: Blocker
* **Affected Scope:** [Lines 186–229](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L186-L229) (Section 2: Automatic Symbolic Shape Deduction) & [Lines 435–443](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L435-L443) (Target File 3)
* **Discrepancy Description:** The design couples an entire frontend DSL shape inference engine (Union-Find dimension deduction, canonical symbol generation, and matrix size binding syntax) with backend BLAS lowering.
* **Rationale:** Automatic shape deduction in the DSL is completely orthogonal to BLAS lowering in the E-Graph. It bloats the implementation plan across 5 frontend files, introduces new failure modes (under-constrained systems, circular contradictions), and creates an unnecessarily complex 21-file changeset.
* ***AI Suggestion:*** PROPOSE FIX
* ***Justification:** Valid critique. Decoupling shape inference keeps the BLAS lowering overhaul focused, disciplined, and low-risk.
* ***Proposed Edit:***
  * Location: Section 2 (Lines 186–229) and Target File 3
  * Change: Excise Section 2 (Automatic Shape Deduction) and matrix size bindings from this document. Retain the explicit `symbols` block from the Pratt parser specification. Defer automatic shape deduction to a dedicated, subsequent frontend RFC.

---

### 6. Critical Omission of 7 Essential Build/Analysis Files in Implementation Plan [`egm-implementation-readiness`] - Severity: Blocker
* **Affected Scope:** [Lines 408–530](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L408-L530) (Detailed Implementation Plan)
* **Discrepancy Description:** The implementation plan omitted 7 essential codebase files required to build, link, and run the project: `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `src/analysis/analysis.h/.cpp`, `src/cost_functions/op_costs.h`, `src/evaluate/evaluator.h`, and `src/cli.cpp`.
* **Rationale:** An engineer executing the plan will experience immediate compilation failures or un-executed tests.
* ***AI Suggestion:*** PROPOSE FIX
* ***Justification:** Valid critique. Enumerating every touched file with exact diff instructions is mandatory for implementation readiness.
* ***Proposed Edit:***
  * Location: Section 9 (Target Files to Create / Modify)
  * Change: Expand the catalog to include all 21 touched files with exact signatures and rationales (as documented in the Stage 9 remediation matrix).

---

### 7. Incomplete Lowering Rulesets & Missing `uplo` in `Op::Symm` [`egm-design`, `egm-implementation-readiness`] - Severity: Major
* **Affected Scope:** [Line 323](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L323) (Table 5) & [Line 500](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L500) (Target File 10)
* **Discrepancy Description:**
  1. `Op::Symm` operand schema omits `uplo` (`[A, B, C, alpha, beta, side]`), but `cblas_dsymm` strictly requires `uplo` (Upper/Lower).
  2. Specifying only 2 TRSM rules omits lower-triangular left solves, upper-triangular right solves, and transpose solves (omitting 6 out of 8 permutations).
  3. GEMV and SYMM rules were omitted from Target File 10.
* **Rationale:** Incomplete rules leave standard matrix expressions unable to lower to BLAS.
* ***AI Suggestion:*** PROPOSE FIX
* ***Justification:** Valid critique.
* ***Proposed Edit:***
  * Location: Table 5 and Target File 10
  * Change:
    1. Update `Op::Symm` operands to: `[A, B, C, alpha, beta, side, uplo]`.
    2. Specify the complete set of 8 rewrite rules covering all combinations of `Side`, `Uplo`, and `Trans` for `Op::Trsm` and `Op::Trmm`.
    3. Add explicit rules for `Op::Gemv` and `Op::Symm`.

---

### 8. Formalize `struct PropertyTag` & Reconcile Name Taxonomy [`egm-codebase-details`, `egm-comprehension`] - Severity: Major
* **Affected Scope:** [Lines 284–309](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L284-L309) (Section 4)
* **Discrepancy Description:** `MatrixFlags` consists of 15 boolean fields (not a bitmask). Tags `square` and `vector` are dynamic shape equality checks, not bitmask flags. Short tag names (`spd`, `upper`) diverge from descriptors in `property_table.h`.
* **Rationale:** Inconsistent naming creates translation overhead, and shape checks cannot be evaluated as flag bits.
* ***AI Suggestion:*** PROPOSE FIX
* ***Justification:** Valid critique.
* ***Proposed Edit:***
  * Location: Section 4
  * Change: Define `struct PropertyTag` in `src/rewrite/pattern.h`:
    ```cpp
    struct PropertyTag {
        enum class Kind { FlagBit, IsSquare, IsVector };
        Kind kind = Kind::FlagBit;
        bool MatrixFlags::*member = nullptr;
        std::string name;
        bool test(const MatrixProperty &prop) const;
    };
    ```
    Support both short names (`spd`, `upper`) and full descriptors (`positive_definite`, `upper_triangular`) as aliases.

---

### 9. In-Place Buffer Mutation Safety for Conditional Bias [`egm-design`, `egm-zoom-out`] - Severity: Major
* **Affected Scope:** [Lines 317–325](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L317-L325) (Table 5) & [Section 7](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L358-L377)
* **Discrepancy Description:** Setting `in_place_input = 2` when $C$ is absent implies mutating a constant buffer or newly allocated node.
* **Rationale:** In-place reuse is only valid when an existing accumulator buffer is provided by the expression ($A \cdot B + C$).
* ***AI Suggestion:*** PROPOSE FIX
* ***Justification:** Valid critique.
* ***Proposed Edit:***
  * Location: Section 5 and Section 7
  * Change: Clarify in `OpDescriptor` that `in_place_input = 2` is conditional on $\beta \ne 0.0$ and $C$ being a mutable matrix operand. When $\beta == 0.0$, in-place buffer reuse is bypassed and a fresh output buffer is allocated.

---

### 10. Disambiguation Fallback for Untyped DSL Identifiers [`egm-design`] - Severity: Major
* **Affected Scope:** [Lines 106–108](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L106-L108) & [Section 6](docs/designs/optional_patterns_and_unified_kernel_lowering.md#L333-L352)
* **Discrepancy Description:** Without `symbols` declarations, the parser cannot distinguish whether an identifier is a scalar or matrix in expressions like `alpha * A`.
* **Rationale:** Semantic disambiguation between `Op::Scale` and `Op::Mul` requires type context.
* ***AI Suggestion:*** PROPOSE FIX
* ***Justification:** Valid critique.
* ***Proposed Edit:***
  * Location: Section 6
  * Change: Document that all identifiers default to `Matrix` unless declared as `Scalar` in the `symbols` block or bound to a scalar numeric literal (`2.0`).

---

### 11. Minor Style & Robustness Fixes [`egm-style`, `egm-comprehension`] - Severity: Nit
* **Affected Scope:** Lines 10, 260–267, 348–349
* **Discrepancy Description:** Inaccurate BLAS level classification for `Gemv`, missing pointer null checks, and silent defaulting on missing scalar variables.
* ***AI Suggestion:*** PROPOSE FIX
* ***Justification:** Low-cost, high-value precision improvements.
* ***Proposed Edit:***
  * Change Line 10 to: "unifies Level 2 and Level 3 BLAS operations...".
  * Add null guards in property lookup utilities.
  * Update `ScalarExpr::evaluate` to throw `std::runtime_error("Unbound scalar variable: " + var)` when missing from `data_bindings`.
