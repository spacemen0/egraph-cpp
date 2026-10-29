# Comprehension Test Audit Report: Optional Pattern Grammar, Symbolic Shape Deduction & Unified BLAS Lowering

**Target Document:** [`docs/designs/optional_patterns_and_unified_kernel_lowering.md`](file:///Users/changguo/Projects/egraph-cpp/docs/designs/optional_patterns_and_unified_kernel_lowering.md)  
**Semantic Boundary:** FeatureRange: lines 1-407, DetailedRange: lines 408-554  
**Reviewer Lens:** `egm-comprehension`  
**Standard:** `resources/document_standards/re-implementation-standard.md`  
**Target Output Path:** `/Users/changguo/Projects/egraph-cpp/comprehension_report.md`

---

## Executive Summary

An external engineer with zero assumed institutional knowledge conducted a clean-slate comprehension and re-implementability audit of the proposed design document. 

The architecture introduces powerful concepts (Union-Find shape deduction, declarative optional pattern expansion, and unified parameterized BLAS kernels). However, an engineer tasked with implementing this design strictly from the specification would encounter **5 Blockers**, **4 Major issues**, and **3 Nits**. Most notably, the proposed formal EBNF grammar cannot parse the design's own canonical rewrite patterns, the dynamic applier generation for absent terms is underspecified, type signatures between `ScalarExpr` and `Operand` contradict existing codebase abstractions, and key CLI/script execution files are omitted from the detailed implementation plan.

| Severity | Count | Primary Impact |
| :--- | :--- | :--- |
| 🔴 **Blocker** | 5 | Cannot parse patterns, compile evaluator callbacks, deduce shapes, or execute CLI |
| 🟡 **Major** | 4 | Incomplete lowering rulesets, signature mismatches, bitmask vs dynamic shape conflation |
| 🟢 **Nit** | 3 | Missing pointer null checks, property tag alias collisions, unary negation precedence |

---

## Prioritized Findings (Blockers -> Major -> Nits)

### 1. Incompatible EBNF Grammar for Pattern Templates - Severity: Blocker
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 158–170 (`Section 1: Formal EBNF Grammar Specification`) vs Lines 50–52 (`CUJ 1`) & Lines 72–74 (`CUJ 3`)
* **Discrepancy Description**:  
  The formal EBNF grammar in Section 1 fails to parse the canonical lowering patterns in CUJ 1 and CUJ 3:
  ```text
  expand_rewrites("gemm", "[?alpha *] [Tr](?a) * [Tr](?b) [+ [?beta *] ?c]", "Gemm(?a, ?b, ?c, ?alpha, ?beta, ?tr_a, ?tr_b)")
  ```
  Specifically:
  1. **Optional operator wrapper `[Tr](?a)`**: `optional_wrapper` is defined as `"[" , pattern_expr , "]"` (line 167), while function calls require an `identifier` followed by `"(" , [ pattern_list ] , ")"` (line 163). In the grammar, an `optional_wrapper` cannot be followed by an argument list `(...)`.
  2. **Trailing operator inside optional clause `[?alpha *]`**: Contains a trailing binary operator `*` without a trailing factor, violating `pattern_term = pattern_factor , { ( "*" | "/" ) , pattern_factor }`.
  3. **Leading operator inside optional clause `[+ [?beta *] ?c]`**: Starts with binary `+` without a preceding term, violating `pattern_expr = pattern_term , { ( "+" | "-" ) , [ pattern_term ] }`.
  4. **Implicit juxtaposition multiplication**: In `[?alpha *] [Tr](?a)`, two factors are juxtaposed without an explicit `*` or `/`. The grammar defines `pattern_term` strictly as requiring explicit operators.
* **Rationale & Recommendation**:  
  An external engineer writing a parser directly from the EBNF specification will immediately encounter syntax errors when parsing the design's primary patterns.  
  Update the EBNF to explicitly specify pattern syntax constructs:
  ```ebnf
  pattern_factor      = optional_op_call
                      | optional_clause
                      | pattern_var
                      | identifier , "(" , [ pattern_list ] , ")"
                      | "(" , pattern_expr , ")"
                      | number_literal
                      | identifier ;
  optional_op_call    = "[" , identifier , "]" , "(" , pattern_list , ")" ;
  optional_clause     = "[" , ( "+" | "-" | "*" ) , pattern_expr , "]"
                      | "[" , pattern_expr , ( "+" | "-" | "*" ) , "]"
                      | "[" , pattern_expr , "]" ;
  pattern_term        = pattern_factor , { [ "*" | "/" ] , pattern_factor } ;
  ```

---

### 2. Underspecified Pattern Expander & Dynamic Applier Synthesis - Severity: Blocker
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 231–278 (`Section 3: Pattern Expansion Subsystem`) & Lines 463–471 (`Target File 6`)
* **Discrepancy Description**:  
  The design specifies `expand_rewrites(name, lhs_template, rhs_template, ...)` taking string views, but leaves the expansion mechanism underspecified:
  1. **Classification of Optional Clauses**: The expander must differentiate between:
     - Boolean operator flags: `[Tr](?a)` $\implies$ binds `?tr_a \in {0, 1}`.
     - Scalar coefficients: `[?alpha *]` $\implies$ defaults `?alpha = 1.0`.
     - Additive bias matrices: `[+ ?c]` $\implies$ synthesizes a zero-matrix applier.  
     The design provides no syntactic rules or metadata explaining how the expander classifies which semantics apply to which brackets.
  2. **Applier Generation**: Lines 258–277 present a hand-written C++ lambda tailored specifically to GEMM (`a_prop`, `b_prop`, `out_rows`, `out_cols`, `make_zero_of_shape`). In C++, lambda source code cannot be synthesized at runtime from a string like `"Gemm(?a, ?b, ?c, ...)"` without an AST interpreter or hardcoded generator.
  3. **Shape Derivation for Missing RHS Variables**: If `?c` is absent on the LHS, how does a generic expander know that `?c` requires shape `(rows(A^transA), cols(B^transB))`? For `Op::Syrk` or `Op::Symm`, the output shapes differ. The design lacks any rule for mapping missing RHS variables to operand dimensions.
* **Rationale & Recommendation**:  
  Without a formal specification, an external implementer cannot know whether `expand_rewrites` is an open-ended DSL compiler or a closed registry of hardcoded kernel templates.  
  Specify that `expand_rewrites` operates via an internal `PatternTemplateAST` with dedicated clause types (`OptionalOp`, `OptionalScalar`, `OptionalBias`), and define how absent variable appliers deduce output shapes from the target `Op` descriptor's registered shape analysis function (`OpDescriptor::analyze`).

---

### 3. Type Conflict & Dropped `ScalarExpr` in `ScalarOperand` & `eval_gemm` - Severity: Blocker
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 326–330 (`Section 5`), Lines 358–377 (`Section 7`), Lines 421–434 (`Target File 2`), and Lines 479–486 (`Target File 8`)
* **Discrepancy Description**:  
  There is a critical contradiction between the proposed scalar architecture and the codebase's operand abstraction:
  1. In `src/core/op_registry.h:44`, `using ScalarOperand = double;`. In current `init_operands` (`src/core/op_registry.cpp:106–108`), encountering a `ScalarExpr` inserts `std::numeric_limits<double>::quiet_NaN()` into `operands`.
  2. In `eval_gemm` (line 370), the design writes: `double alpha = as_scalar(operands[3]).value_or(1.0);`. However, `as_scalar` returns `const ScalarOperand *` (`const double *`), which has no `.value_or(...)` member.
  3. In line 485, the plan states: *"evaluate symbolic `ScalarExpr` operands against `data_bindings` via `scalar_expr.evaluate(data_bindings)`"*. But because `operands[3]` is a `double` (or `NaN`), the `ScalarExpr` has already been discarded during `init_operands`.
  4. The implementation plan for `src/core/op_registry.h` (lines 421–434) completely omits updating `ScalarOperand` to hold `ScalarExpr`.
* **Rationale & Recommendation**:  
  An implementer compiling `eval_gemm` will encounter compilation failures (`value_or` does not exist on pointer) or runtime bugs (operands receive `NaN`).  
  Update `src/core/op_registry.h` to define `using ScalarOperand = ScalarExpr;` (or `std::variant<double, ScalarExpr>`), update `init_operands` to extract the `ScalarExpr` atom from the leaf E-node, and update `eval_gemm` to call `operands[3]->evaluate(data_bindings)`.

---

### 4. Missing Identifier-to-Dimension Mapping in `ShapeInferenceContext` - Severity: Blocker
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 188–210 (`Section 2: Disjoint-Set Dimension Unification`) & Lines 435–443 (`Target File 3`)
* **Discrepancy Description**:  
  The interface for `ShapeInferenceContext` defines:
  ```cpp
  SizeBindings resolve_bindings(const std::unordered_map<std::string, std::pair<int, int>> &matrix_sizes) const;
  ```
  However, the private members of `ShapeInferenceContext` are listed as:
  ```cpp
  private:
      std::vector<int> parent;
      std::unordered_map<int, std::string> canonical_names;
  ```
  The context stores no mapping between matrix identifiers (e.g. `"X"`, `"y"`) and their constituent dimension equivalence class IDs (`dim_row`, `dim_col`). When `resolve_bindings` is invoked with `sizes(X = 1000x20)`, the context has no way to look up which dimension class corresponds to `rows(X)` or `cols(X)`.
* **Rationale & Recommendation**:  
  An external engineer implementing `ShapeInferenceContext` cannot fulfill the contract of `resolve_bindings`.  
  Add `std::unordered_map<std::string, std::pair<int, int>> matrix_dimensions;` and a registration method `void register_matrix(std::string_view name, int row_dim, int col_dim)` to `ShapeInferenceContext`.

---

### 5. Missing CLI Runner and Program AST in Detailed Implementation Plan - Severity: Blocker
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 66–68 (`CUJ 2`), Lines 410–530 (`Target Files to Create / Modify`), and Lines 546–548 (`Verification Plan`)
* **Discrepancy Description**:  
  CUJ 2 and the Verification Plan require running script files via:
  ```bash
  egraph_cli run tests/fixtures/ols_auto_shape.eg
  ```
  However:
  1. `src/cli.cpp` is not listed in the Detailed Implementation Plan (Target Files 1–14).
  2. Inspecting `src/cli.cpp` reveals that `main(int argc, char **argv)` contains `(void)argc; (void)argv;` and runs only an interactive REPL with no `run` command or file argument parsing.
  3. Target File 4 claims that `src/common/parser.h` will modify `parse_program()`. But `src/common/parser.h` currently contains only `parse_expression` and `parse_scalar`. No AST types or driver functions for programs (`ProgramAST`, `OperationDecl`, `ExecutionPlan`) are defined or scheduled for creation.
* **Rationale & Recommendation**:  
  The end-to-end verification step (`egraph_cli run ...`) is impossible to execute without modifying `cli.cpp`.  
  Add `src/cli.cpp` as an explicit target file in the Detailed Implementation Plan, and define `struct ProgramAST` and `parse_program(std::string_view)` in `src/common/parser.h`.

---

### 6. Function Signature Mismatch on Kernel Evaluator Callback (`EvalFn`) - Severity: Major
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 359–362 (`Section 7`) vs `src/core/op_registry.h:62–64` & Lines 421–434 (`Target File 2`)
* **Discrepancy Description**:  
  In `src/core/op_registry.h`, `EvalFn` is declared as:
  ```cpp
  using EvalFn = std::function<void(
      std::string_view op_name, const std::vector<Operand> &operands,
      const std::vector<const MatrixNode *> &inputs, MatrixNode &output)>;
  ```
  In Section 7, `eval_gemm` is defined with five parameters:
  ```cpp
  void eval_gemm(
      std::string_view op_name, const std::vector<Operand> &operands,
      const std::vector<const MatrixNode *> &inputs, MatrixNode &output,
      const DataBindings &data_bindings);
  ```
  The implementation plan for `src/core/op_registry.h` does not include updating `EvalFn` to take `const DataBindings &`. Without updating the `typedef`, `eval_gemm` cannot be assigned to `OpDescriptor::eval`.
* **Rationale & Recommendation**:  
  Update `OpDescriptor::EvalFn` in `src/core/op_registry.h` to accept `const DataBindings &data_bindings`, or document how `data_bindings` are passed down to kernel execution routines.

---

### 7. Incomplete TRSM Lowering Ruleset Coverage & Hardcoded Enums - Severity: Major
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 72–74 (`CUJ 3`), Line 321 (`Table 5`), and Line 500 (`Target File 10`)
* **Discrepancy Description**:  
  Line 500 specifies: *"Replace TRSM rules with 2 declarative rules (`trsm_left` and `trsm_right`)."*  
  The rules in CUJ 3 are:
  - `expand_rewrites("trsm_left", "[?alpha *] Inv(?a{upper}) * ?b", "Trsm(?a, ?b, ?alpha, Side::Left, Uplo::Upper, 0)")`
  - `expand_rewrites("trsm_right", "[?alpha *] ?b * Inv(?a{lower})", "Trsm(?a, ?b, ?alpha, Side::Right, Uplo::Lower, 0)")`  
  This leaves significant functional gaps:
  1. `Inv(?a{lower}) * ?b` (left solve with lower triangular matrix) is omitted.
  2. `?b * Inv(?a{upper})` (right solve with upper triangular matrix) is omitted.
  3. Transposed solves (`Tr(Inv(?a)) * ?b` and `?b * Tr(Inv(?a))`) are omitted.  
  Table 5 states that `Op::Trsm` collapses `Trsm_LN`, `Trsm_LT`, `Trsm_RN`, and `Trsm_RT`. With only 2 rules, half of the triangular configurations and all transpose configurations will fail to lower.
* **Rationale & Recommendation**:  
  Specify the complete set of rewrite rules required to cover all 8 permutations of Side (Left/Right), Uplo (Upper/Lower), and Transpose (NoTrans/Trans) for `Op::Trsm` and `Op::Trmm`.

---

### 8. Undefined Scoped Enum Constants in Pattern RHS Grammar - Severity: Major
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 73–74 (`CUJ 3`), Lines 158–170 (`Section 1`), and Line 327 (`Section 5`)
* **Discrepancy Description**:  
  CUJ 3 passes scoped enum identifiers directly into the RHS template string:
  `"Trsm(?a, ?b, ?alpha, Side::Left, Uplo::Upper, 0)"`.
  1. The grammar for patterns (`Section 1`) only recognizes simple `identifier` tokens; it contains no production for qualified names with `::`.
  2. Section 5 states: *"Flags (`transA`, `side`, `uplo`) are interned as constant integer E-nodes (`Atom(int)`)."* The design nowhere specifies the integer values that `Side::Left`, `Side::Right`, `Uplo::Upper`, and `Uplo::Lower` map to (e.g. CBLAS constants `141/142`, `121/122`, or boolean flags `0/1`).
* **Rationale & Recommendation**:  
  Update the pattern grammar to support scoped identifiers (`ident "::" ident`), and explicitly define the enumeration values used when interning `Side` and `Uplo` constants into the E-Graph.

---

### 9. Property Tag Bitmask vs. Dynamic Shape Predicate Inconsistency - Severity: Major
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 284–309 (`Section 4`) & Lines 459–460, 476–478 (`Target Files 5 & 7`)
* **Discrepancy Description**:  
  Section 4 catalogues property tags and claims in Line 477: *"Validate flags (`is_upper_triangular`, `is_symmetric`, etc.) using bitwise operations without intermediate string allocations."*
  However:
  1. In `src/analysis/property_table.h`, `MatrixFlags` is a struct containing 15 discrete `bool` members, not a bitmask.
  2. Tags `square` and `vector` map to dynamic shape helper methods (`is_square()`, `is_vector()`), which inspect `shape.first` and `shape.second`. They cannot be tested via a bitwise flag check against `flags`.
  3. The structure of `PropertyTag` and the signature of `tag.test(*prop)` (line 305) are never formally defined.
* **Rationale & Recommendation**:  
  Define `struct PropertyTag`:
  ```cpp
  struct PropertyTag {
      enum class Kind { FlagBit, IsSquare, IsVector };
      Kind kind;
      uint32_t flag_mask = 0;
      bool test(const MatrixProperty &prop) const;
  };
  ```
  Clarify how `MatrixFlags` is either converted to a bitfield or how `tag.test` delegates between boolean flag testing and shape dimension comparison.

---

### 10. Missing Null Check in Dynamic Applier Code Template - Severity: Nit
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 261–267 (`Section 3`)
* **Discrepancy Description**:  
  In the dynamic applier lambda snippet:
  ```cpp
  const auto *a_prop = get_matrix_data(g, a_id);
  const auto *b_prop = get_matrix_data(g, b_id);
  Size out_rows = (tr_a == 0) ? a_prop->shape.first : a_prop->shape.second;
  ```
  `get_matrix_data` returns a raw pointer which may be `nullptr` if analysis has not run on the node. Dereferencing `a_prop` without a null check will trigger a segmentation fault.
* **Rationale & Recommendation**:  
  Add null-check guards (`if (!a_prop || !b_prop) return {0, false};`) in the applier template to prevent crashes on unanalyzed nodes.

---

### 11. Property Tag Short Names vs. Existing Descriptor Collision - Severity: Nit
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 284–295 (`Section 4`) vs `src/analysis/property_table.h:44–60`
* **Discrepancy Description**:  
  In `property_table.h`, existing property descriptors use long names: `"upper_triangular"`, `"lower_triangular"`, `"positive_definite"`, `"full_rank"`, `"non_singular"`.  
  The design introduces abbreviated names (`upper`, `lower`, `spd`, `fullrank`, `nonsingular`). The plan does not specify whether the long names are deprecated, aliased, or replaced, creating ambiguity for existing test fixtures and scripts.
* **Rationale & Recommendation**:  
  Explicitly specify that `MatrixProperty::flag_descriptors` will support both canonical short names and backward-compatible long names as aliases.

---

### 12. Underspecified Unary vs. Binary Negation in Pattern Pratt Parser - Severity: Nit
* **Reviewer Lens:** `egm-comprehension`
* **Affected Scope:** Lines 148–150, 159 (`Section 1`)
* **Discrepancy Description**:  
  In Section 1, `prefix_op = "-"` and `pattern_expr = pattern_term , { ( "+" | "-" ) , [ pattern_term ] }`. The grammar does not define binding power precedence for unary negation vs binary subtraction within pattern expressions (e.g. `-?a * ?b` vs `?a - ?b`).
* **Rationale & Recommendation**:  
  Document the Pratt parsing precedence table for prefix negation versus infix operators in the pattern parser specification.
