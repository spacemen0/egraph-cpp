# Goldfish Review Report

**Target:** `/Users/changguo/Projects/egraph-cpp/docs/designs/optional_patterns_and_unified_kernel_lowering.md` (`lines 1-407`)  
**Reviewers:** `[egm-design, egm-structural, egm-style]`  
**Standards:** `feature-spec-standard`  
**Severity Breakdown:** 🔴 5 Blockers | 🟡 6 Majors | 🟢 4 Nits  

---

## Executive Summary
This document proposes a major clean-slate architecture unifying pattern optionality, symbolic shape inference via Union-Find, and universal BLAS kernel lowering. While the high-level intent is ambitious and sound, an antagonistic audit of lines 1–407 reveals critical compilation errors in sample code, severe structural breakages in the EBNF grammar that make the headline pattern syntax impossible to parse, runtime logic bugs in dynamic appliers, and missing operand parameters that violate BLAS semantics.

---

## Prioritized Findings (Blockers -> Major -> Nits)

### 1. Broken EBNF Grammar for Pattern Optionality (`[+ ?c]`, `[?alpha *]`, `[Tr](?a)`) - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-design` / `egm-structural`
* **Affected Scope:** Lines 159–170, 51–53, 23–24
* **Discrepancy Description**: 
  The formal EBNF grammar fails to parse the very optional patterns presented as primary user journeys and design goals:
  1. `optional_wrapper = "[" , pattern_expr , "]" ;` requires the content between brackets to be a valid `pattern_expr`. However, `[+ ?c]` starts with a leading binary operator `+` (no unary `+` exists in `pattern_factor`), making `+ ?c` syntactically invalid.
  2. `[?alpha *]` contains a trailing binary operator `*`. The rule `pattern_term = pattern_factor , { ( "*" | "/" ) , pattern_factor }` requires a right-hand operand; thus `?alpha *` cannot be parsed.
  3. `[Tr](?a)` places an argument list `(?a)` immediately after an `optional_wrapper`. In the grammar, function calls are defined strictly as `identifier , "(" , [ pattern_list ] , ")"`. An `optional_wrapper` cannot be invoked as a callable function.
  4. In `pattern_expr = pattern_term , { ( "+" | "-" ) , [ pattern_term ] } ;`, `pattern_term` is marked optional, which erroneously permits dangling operators (e.g. `a +` or `a -`) as valid expressions.
* **Rationale & Recommendation**: 
  A design specification cannot have an EBNF grammar that rejects its primary advertised syntax. Restructure the pattern grammar to explicitly specify optional operator affixes and optional wrappers:
  ```ebnf
  optional_wrapper    = "[" , [ ( "+" | "-" ) ] , pattern_expr , "]"
                      | "[" , pattern_expr , ( "*" | "/" ) , "]" ;
  optional_call       = "[" , identifier , "]" , "(" , [ pattern_list ] , ")" ;
  ```
  Ensure dangling binary operators are strictly disallowed in standard `pattern_expr`.

---

### 2. Unbound Variable Defaulting Gap in Expanded Pattern Templates - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Lines 51–56, 233–278
* **Discrepancy Description**: 
  CUJ 1 expands `"[?alpha *] [Tr](?a) * [Tr](?b) [+ [?beta *] ?c]"` into standard rewrite rules targeting RHS `"Gemm(?a, ?b, ?c, ?alpha, ?beta, ?tr_a, ?tr_b)"`.
  When `?c` is present (e.g., matching `A * B + C`) but `[?alpha *]` or `[?beta *]` is omitted in the input expression, `?alpha` and `?beta` are not matched on the LHS and do not exist in the `Substitution` map. 
  In `egraph-cpp`, pattern instantiation (`Pattern::instantiate`) does not support defaulting missing substitution variables; attempting to instantiate `?alpha` or `?beta` will either fail with a missing key exception or generate orphaned variable nodes. 
  The dynamic applier (lines 258–278) only handles branches where `[+ ?c]` is omitted. For all branches where `?c` is present but scalar factors are absent, the RHS instantiation crashes or creates invalid graphs.
* **Rationale & Recommendation**: 
  Specify whether `expand_rewrites` bakes constant literal nodes into the RHS pattern string during compile-time expansion (e.g. rewriting the template RHS directly to `"Gemm(?a, ?b, ?c, 1.0, 1.0, ?tr_a, ?tr_b)"` for that branch), or uses a dynamic applier for all expanded permutations with explicit fallback lookups (`s.value_or("alpha", default_alpha_id)`).

---

### 3. Fatal Flaw in Flag Binding Variable Resolution (`s.count("tr_a")` in Dynamic Applier) - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Lines 233–238, 263–264, 272–273
* **Discrepancy Description**: 
  Section 3 claims:
  > "When the expander encounters `[Op](?var)`: The flag variable is automatically derived as `?op_var`... In the active branch: `?tr_a` is bound to interned E-node 1. In the bypassed branch: `?tr_a` is bound to interned E-node 0."
  
  However, the dynamic applier code snippet directly inspects the runtime substitution map `s`:
  ```cpp
  int tr_a = s.count("tr_a") ? get_int_from_eclass(g, s.at("tr_a")).value_or(0) : 0;
  Id ta_id = s.count("tr_a") ? s.at("tr_a") : g.add_node(ENode({}, 0));
  ```
  `Matcher` only binds variables physically present in the LHS pattern. In the expanded LHS `Tr(?a) * ?b`, `?tr_a` is NOT a variable in the pattern. Consequently, `s.count("tr_a")` will ALWAYS evaluate to `false` (0) at runtime. The dynamic applier will always set `tr_a = 0`, even when `Tr(?a)` was matched.
* **Rationale & Recommendation**: 
  The expansion generator must capture the flag value directly in the synthesized C++ lambda closure (e.g. `[tr_a_val = 1, tr_b_val = 0](...)`) rather than querying the `Substitution` map for non-existent pattern variables.

---

### 4. Invalid C++ Compilation in CBLAS Evaluator and Missing `DataBindings` Parameter in `EvalFn` - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Lines 358–377, 480–486
* **Discrepancy Description**: 
  The CBLAS dispatch code in `eval_gemm` contains multiple fatal C++ compilation and architectural errors:
  1. `as_scalar(operands[3]).value_or(1.0)` will not compile. In `src/core/op_registry.h:57`, `as_scalar` returns a raw pointer `const ScalarOperand*` (`const double*`), which has no `.value_or()` member.
  2. `ScalarOperand` is defined as `double`. In `src/core/op_registry.cpp:107`, symbolic scalars are replaced with `NaN` by `init_operands`. `Operand` has no representation for `ScalarExpr`.
  3. In `src/core/op_registry.h:62`, `EvalFn` is defined as:
     `using EvalFn = std::function<void(string_view, const vector<Operand>&, const vector<const MatrixNode*>&, MatrixNode&)>;`
     It does NOT accept `const DataBindings &data_bindings`. 
  4. Detailed Implementation Plan item 8 (line 485) claims `eval_gemm` evaluates `scalar_expr.evaluate(data_bindings)`, but neither `ScalarExpr` nor `data_bindings` is accessible from `operands`.
* **Rationale & Recommendation**: 
  - Redefine `ScalarOperand` in `op_registry.h` to `std::variant<double, ScalarExpr>` (or add `ScalarExpr` as a first-class `Operand` variant).
  - Update `EvalFn` in `op_registry.h` to accept `const DataBindings &data_bindings`.
  - Fix the operand access in `eval_gemm`: check `as_scalar(operands[3])` for nullptr and evaluate `ScalarExpr` if present.

---

### 5. Null Pointer Dereference Risk in Dynamic Applier (`get_matrix_data` Unchecked) - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Lines 258–268
* **Discrepancy Description**: 
  The synthesized dynamic applier directly dereferences pointers returned by `get_matrix_data`:
  ```cpp
  const auto *a_prop = get_matrix_data(g, a_id);
  const auto *b_prop = get_matrix_data(g, b_id);
  Size out_rows = (tr_a == 0) ? a_prop->shape.first : a_prop->shape.second;
  ```
  If `a_id` or `b_id` corresponds to an e-class holding a non-matrix property (such as a scalar or tuple) or an intermediate e-class whose property analysis has not yet converged, `get_matrix_data` returns `nullptr`. The applier will trigger an immediate segmentation fault (SIGSEGV).
* **Rationale & Recommendation**: 
  Add explicit null-checks guarding the shape lookup:
  ```cpp
  if (!a_prop || !b_prop) {
      return std::make_pair(Id(0), false);
  }
  ```
  Alternatively, mandate that `expand_rewrites` attaches condition guards (`is_matrix("a") && is_matrix("b")`) to every expanded rewrite rule.

---

### 6. Combinatorial Permutation Count Inconsistency (16 vs 24 vs 32 Rules) - Severity: 🟡 Major
* **Reviewer Lens:** `egm-structural` / `egm-design`
* **Affected Scope:** Lines 51–56, 16, 36
* **Discrepancy Description**: 
  CUJ 1 states that `expand_rewrites("gemm", "[?alpha *] [Tr](?a) * [Tr](?b) [+ [?beta *] ?c]", ...)` expands into "16 concrete rules".
  However, counting the optional elements in this template:
  - `[?alpha *]` (2 states)
  - `[Tr](?a)` (2 states)
  - `[Tr](?b)` (2 states)
  - `[+ [?beta *] ?c]` contains an outer optional addition and an inner optional scalar factor.
  A standard Cartesian expansion across 5 independent binary options yields $2^5 = 32$ permutations. If pruned hierarchically (omitting `beta` when `+` is absent), it yields $2 \times 2 \times 2 \times (1 + 2) = 24$ permutations. Neither formula produces 16. The document contradicts basic combinatorics and its own rule count limits (line 36 states "maximum 32 expanded rewrites").
* **Rationale & Recommendation**: 
  Explicitly define the pruning rules for nested optionals in Section 3 and correct the rule count in CUJ 1 to 24 (if pruned) or 32 (if unpruned Cartesian product).

---

### 7. Syntax Invalidity and Unmapped Enum Qualifiers in TRSM Pattern (`Side::Left`, `Uplo::Upper`) - Severity: 🟡 Major
* **Reviewer Lens:** `egm-design` / `egm-structural`
* **Affected Scope:** Lines 73–75, 163–166, 321, 327
* **Discrepancy Description**: 
  In CUJ 3, the rewrite templates use C++ scoped enum qualifiers inside pattern strings:
  `expand_rewrites("trsm_left", "[?alpha *] Inv(?a{upper}) * ?b", "Trsm(?a, ?b, ?alpha, Side::Left, Uplo::Upper, 0)")`
  In Section 1 EBNF and `parser.cpp:85–92`, identifiers cannot contain `::`. The string `Side::Left` is a syntax error.
  Furthermore, Section 5 notes that flags are interned as constant integer E-nodes (`Atom(int)`), but nowhere does the document define the integer mapping for `Side::Left` vs `Side::Right` and `Uplo::Upper` vs `Uplo::Lower`. In the same signature, `trans` is passed as literal `0` while `side` and `uplo` are passed as unquoted C++ qualifiers.
* **Rationale & Recommendation**: 
  Define standard string identifiers or integer literals for BLAS kernel flags in patterns (e.g. `"left"`, `"right"`, `"upper"`, `"lower"`, or canonical integers `0, 1`), and document their exact mapping to CBLAS enums.

---

### 8. Missing `uplo` Flag in `Op::Symm` Kernel Layout (BLAS Semantic Incompleteness) - Severity: 🟡 Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Lines 323, 432
* **Discrepancy Description**: 
  Table 5 defines the child operand layout for `Op::Symm` as:
  `[A, B, C, alpha, beta, side]`
  In standard BLAS (`cblas_dsymm`), symmetric matrix multiplication requires both `side` (Left/Right) and `uplo` (Upper/Lower), because symmetric matrices are stored as upper or lower triangular matrices. In `src/evaluate/evaluator.cpp:360`, `cblas_dsymm` takes `side` and `uplo_cblas`. 
  `Op::Trsm` and `Op::Trmm` include both `side` and `uplo` in Table 5, but `Op::Symm` omits `uplo`. Without `uplo`, the evaluator cannot determine which triangular half of `A` contains valid matrix data.
* **Rationale & Recommendation**: 
  Update the operand layout of `Op::Symm` to include `uplo`:
  `[A, B, C, alpha, beta, side, uplo]`.

---

### 9. Ambiguity in Dimension Inference vs Parse-Time Disambiguation of Scalars (`Op::Scale`) - Severity: 🟡 Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Lines 106–108, 186–229
* **Discrepancy Description**: 
  G3 and D3 make the `symbols` block optional. In an untyped script:
  `W_next = W - alpha * Grad;`
  Neither the Pratt parser nor `ShapeInferenceContext` can determine whether `alpha` is a `Scalar` (scaling `Grad`) or a `Matrix` (matrix multiplication).
  If shape inference runs equational unification assuming `alpha * Grad` is a matrix product, it will unify `cols(alpha) == rows(Grad)`.
  Architecture diagram line 107 mentions "Parse-time Op::Scale Disambiguation", but the text provides zero mechanism for how this disambiguation occurs without type declarations.
* **Rationale & Recommendation**: 
  Explicitly specify the disambiguation rules: e.g., identifiers default to `Matrix` unless declared in `symbols` as `Scalar` or bound to a scalar numeric literal, or specify an equational fallback if a dimension class collapses to $1 \times 1$.

---

### 10. In-Place Buffer Reuse Danger on Synthesized Zero Matrices - Severity: 🟡 Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Lines 317–325, 268–275
* **Discrepancy Description**: 
  Table 5 designates `in_place_input = 2` (`C`) for `Op::Gemm`.
  When `[+ ?c]` is omitted on the LHS, `make_zero_of_shape` synthesizes a zero matrix node for `C`. If the zero matrix e-class is shared across multiple operations (which is typical in equality saturation where zero constants are deduped), executing `Op::Gemm` in-place will overwrite `C` in memory, corrupting all other expressions reading from that zero matrix node.
* **Rationale & Recommendation**: 
  Specify that dynamic zero appliers must either mark synthesized zero buffers as non-reusable/unique, or require the evaluator memory planner to verify that `use_count == 1` before allowing in-place buffer reuse.

---

### 11. Evaluation Directive Lacks `sizes(...)` Binding Clause in Grammar - Severity: 🟡 Major
* **Reviewer Lens:** `egm-structural` / `egm-design`
* **Affected Scope:** Lines 174–175, 84, 348
* **Discrepancy Description**: 
  In Section 1 EBNF:
  - `extract` allows `[ "with" , "sizes" , "(" , size_binding_list , ")" ]`.
  - `evaluate` allows only `[ "with" , "data" , "(" , data_binding_list , ")" ]`.
  However, `ScalarExpr::evaluate` (line 348) accepts `const SizeBindings &sizes = {}`, and `Evaluator` requires concrete matrix dimensions to allocate buffers for execution. If a user script executes `evaluate` on a pipeline without a preceding `extract` command, dimension sizes cannot be provided.
* **Rationale & Recommendation**: 
  Update EBNF line 175 to allow `evaluate` to accept both `sizes` and `data`:
  ```ebnf
  | "evaluate" , identifier , [ "with" , "data" , "(" , data_binding_list , ")" ] , [ "with" , "sizes" , "(" , size_binding_list , ")" ] , ";" ;
  ```

---

### 12. Inaccurate BLAS Level Classification (`Gemv` is Level 2, not Level 3) - Severity: 🟢 Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** Line 10
* **Discrepancy Description**: 
  Line 10 states: "...unifies Level 3 BLAS operations into canonical parameterized kernels (`Op::Gemm`, `Op::Syrk`, `Op::Trsm`, `Op::Trmm`, `Op::Symm`, `Op::Gemv`)".
  `Gemv` is a matrix-vector product, which is BLAS Level 2, not Level 3.
* **Rationale & Recommendation**: 
  Change phrasing to: "unifies Level 2 and Level 3 BLAS operations into canonical parameterized kernels...".

---

### 13. Inconsistent ENode Construction Syntax in Examples - Severity: 🟢 Nit
* **Reviewer Lens:** `egm-style` / `egm-structural`
* **Affected Scope:** Lines 27, 271–275
* **Discrepancy Description**: 
  Line 27 writes `ENode({}, ScalarExpr("alpha"))` (children first, atom second).
  Line 272 writes `ENode({}, 0)`.
  Line 275 writes `ENode({a_id, b_id, ...}, OpRegistry::op("Gemm"))`.
  In `egraph-cpp`, `ENode` constructor parameter ordering should be presented consistently across all documentation snippets.
* **Rationale & Recommendation**: 
  Standardize on `ENode(Children children, Atom atom)` or `ENode(Atom atom, Children children)` consistently throughout all code snippets.

---

### 14. Undefined Type `Size` in Dynamic Applier Snippet - Severity: 🟢 Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** Lines 266–267
* **Discrepancy Description**: 
  Lines 266–267 declare `Size out_rows = ...; Size out_cols = ...;`. In `egraph-cpp`, dimensions are represented as `int` or `std::variant<int, std::string>`. `Size` is not an established type in `basic_types.h`.
* **Rationale & Recommendation**: 
  Replace `Size` with `int` or `Dimension`.

---

### 15. Redundant Semicolons Allowed in `property_flags` EBNF - Severity: 🟢 Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** Line 140
* **Discrepancy Description**: 
  `property_flags = "{" , identifier , { ( "," | ";" ) , identifier } , "}" ;`
  Permitting semicolons inside curly braces for property tags (`{upper; spd}`) is unconventional and visually inconsistent with standard comma separation (`{upper, spd}`).
* **Rationale & Recommendation**: 
  Simplify production to: `property_flags = "{" , identifier , { "," , identifier } , "}" ;`.
