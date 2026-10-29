# Intermediate Zoom Out Review Report (Stage 8)

**Target Document:** [`docs/designs/optional_patterns_and_unified_kernel_lowering.md`](file:///Users/changguo/Projects/egraph-cpp/docs/designs/optional_patterns_and_unified_kernel_lowering.md) (Lines 1–555)  
**Reviewer:** `egm-zoom-out` (Stage 8 Intermediate Zoom Out Review)  
**Standard:** `resources/document_standards/zoom-out-standard.md` (Simplicity, Architectural Bloat, YAGNI, Component Reuse)  
**Context Integrated:** [`zoom_out_report.md`](file:///Users/changguo/Projects/egraph-cpp/zoom_out_report.md), [`critic_report.md`](file:///Users/changguo/Projects/egraph-cpp/critic_report.md), [`comprehension_report.md`](file:///Users/changguo/Projects/egraph-cpp/comprehension_report.md)  
**Output Target:** `/Users/changguo/Projects/egraph-cpp/intermediate_zoom_out_report.md`  

---

## Executive Summary

Stage 8 provides an intermediate high-altitude architectural steering evaluation of the clean-slate design document, synthesizing findings from the initial architectural critique (Stage 4), the design critic audit (Stage 5, 5 Blockers), and the clean-slate comprehension audit (Stage 6, 5 Blockers).

### High-Altitude Verdict: Monolithic Over-Reach & Complexity Inversion

The design document set out to resolve a well-scoped and pressing problem: **the combinatorial explosion of hand-written BLAS lowering rules in `src/rule_sets/lowering.h` and fragmented enum kernels in `src/core/op_registry.h`**.

However, by attempting to solve three completely decoupled problems simultaneously, the design has ballooned into an unwieldy, high-risk, 14-file compiler-wide overhaul:
1. **Frontend DSL Ergonomics**: Automatic equational shape deduction via Union-Find and matrix size binding syntax (`sizes(X=1000x20)`).
2. **Rewrite Engine Metaprogramming**: A novel macro language (`[Tr](?a)`, `[+ ?c]`), Cartesian template expander, and procedural zero-matrix lambda synthesis.
3. **Core BLAS Unification**: Collapsing enum kernels (`Op::Gemm`, `Op::Trsm`, etc.), property constraint matching (`?a{upper}`), and closed-form scalar operands.

The 10 cumulative blockers identified by the Critic and Comprehension reviews are **direct symptoms of this architectural over-reach**. When evaluated through the lenses of **Simplicity**, **YAGNI**, and **Component Reuse**, it is evident that:
* **Shape Deduction is Textbook YAGNI**: DSL shape inference is completely orthogonal to BLAS lowering in the E-Graph.
* **The Pattern Expander Inverts Net Complexity**: Inventing a domain-specific macro grammar and dynamic zero-matrix appliers introduces far more operational complexity than the 20 rules it seeks to eliminate. Crucially, CBLAS handles absent bias natively via $\beta = 0.0$ without allocating zero matrices.
* **Codebase Primitives are Ignored**: The design re-invents Union-Find (ignoring `src/core/union_find.h`) and `ScalarExpr` (already in `src/core/basic_types.h`), while overlooking the actual type defect in `src/core/op_registry.h` (`using ScalarOperand = double`).

---

## Severity Breakdown

| Severity | Count | Primary Impact |
| :--- | :---: | :--- |
| 🔴 **Blocker** | 2 | Monolithic scope conflation, complexity inversion via synthetic zero-matrix appliers |
| 🟡 **Major** | 3 | Wheel reinvention (`UnionFind`, `ScalarExpr`), property taxonomy clash, in-place buffer contradictions |
| 🟢 **Nit** | 2 | Scalar fallback policies, test suite proliferation |

---

## Prioritized Findings (Blockers -> Major -> Nits)

### 1. Monolithic Scope Conflation: Decouple Frontend Shape Deduction from BLAS Lowering - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-zoom-out`
* **Affected Scope:** Lines 10–13, 25–26 (G3–G4), 101–111, 186–229 (Section 2), 435–443 (Target File 3), 448–454 (Target File 4), 503–510 (Target File 11)
* **Discrepancy Description**:  
  The proposal bundles a frontend DSL type inference engine (`ShapeInferenceContext`, Disjoint-Set dimension unification, canonical symbol generation, and matrix-based size binding syntax) with a backend BLAS kernel unification and rewrite lowering overhaul.
  
  The Critic and Comprehension audits proved that this subsystem introduces severe fragility:
  - Underspecified dimension mapping from matrix names to dimension variables (Comprehension Blocker 4).
  - Failure to handle circular constraints, dimension contradictions, and under-constrained systems (Critic Major 1).
  - Unhandled CLI and execution pipeline modifications (Comprehension Blocker 5).
  - Bloats implementation scope across 5 frontend files (`parser.h`, `parser.cpp`, `shape_inference.h`, `shape_inference.cpp`, `shape_inference_test.cpp`).
* **Rationale & Recommendation**:  
  At 30,000 feet, **automatic shape deduction has zero operational connection to BLAS kernel lowering**. Lowering rules operate inside the E-Graph on expressions that are already parsed and typed. Equational shape deduction solves a separate problem (user convenience in authoring DSL scripts without declaring `symbols`). 
  
  Conflating the two creates a high-risk changeset that turns an engine cleanup into an overhaul of the entire parsing, analysis, and execution pipeline.
  
  **Recommendation**:  
  **Excise Section 2 (Automatic Shape Deduction) and matrix size bindings completely from this specification.** Defer shape deduction to a dedicated, subsequent frontend RFC. Keep the `symbols` block explicit for now. This immediately eliminates 3 Blockers and 2 Majors from the implementation backlog.

---

### 2. Complexity Inversion & Leaky Abstractions in `expand_rewrites` vs Native BLAS Semantics - Severity: 🔴 Blocker
* **Reviewer Lens:** `egm-zoom-out`
* **Affected Scope:** Lines 23–24 (G1), 29–30 (G7), 158–170, 231–278 (Section 3), 463–471 (Target File 6), 511–519 (Target File 12)
* **Discrepancy Description**:  
  To eliminate ~20 hand-written lowering rules, the design introduces:
  1. A pattern grammar extension (`[Tr](?a)`, `[+ ?c]`, `[?alpha *]`) that breaks EBNF parsing (Critic Blocker 1, Comprehension Blocker 1).
  2. Synthesized dynamic C++ appliers that query matrix metadata and call `make_zero_of_shape` (Critic Blocker 5, Comprehension Blocker 2).
  3. A flag resolution mechanism (`s.count("tr_a")`) that is fundamentally broken because pattern matchers do not bind absent variables (Critic Blocker 3).
  4. Variable defaulting gaps where omitted variables (`?alpha`) cause RHS pattern instantiation crashes (Critic Blocker 2).
  
  **High-Altitude Insight**: In CBLAS, computing $A \cdot B$ without bias is achieved simply by passing $\beta = 0.0$ to `cblas_dgemm`. CBLAS does not read or require a pre-allocated zero matrix; $C$ is strictly an output buffer when $\beta = 0$. By forcing an explicit zero-matrix E-node into the E-Graph via `make_zero_of_shape`, the design creates an artificial dependency on matrix property data, allocates synthetic zero nodes in equality saturation, and breaks in-place buffer annotations.
* **Rationale & Recommendation**:  
  The operational complexity introduced across the parser, AST, pattern expander, and dynamic applier exceeds the complexity of the rules being replaced by an order of magnitude. 
  
  Once BLAS kernels are unified (`Op::Gemm`) and property tags (`?a{upper}`) are added, the number of rules required for GEMM drops from 32 to **4 explicit rules** (representing the 4 transpose permutations $NN, TN, NT, TT$).
  
  **Recommendation**:  
  1. **Do not synthesize zero matrices.** Set the scalar operand $\beta = 0.0$ in `Op::Gemm`. In `eval_gemm`, when $\beta == 0.0$, the evaluator passes `beta = 0.0` directly to `cblas_dgemm` without reading or allocating a zero matrix.
  2. **Replace the macro pattern language and dynamic applier synthesizer** with a compact, deterministic C++ helper function or loop inside `src/rule_sets/lowering.h` that emits the concrete `Rewrite` instances for transpose combinations, or keep explicit pattern definitions. This eliminates the need for `OptionalPatternNode`, macro EBNF grammar, and Cartesian expander engines.

---

### 3. Component Reinvention vs Existing Codebase Primitives (`UnionFind`, `ScalarExpr`, `ScalarOperand`) - Severity: 🟡 Major
* **Reviewer Lens:** `egm-zoom-out`
* **Affected Scope:** Lines 207–210 (Section 2), 333–350 (Section 6), 412–420 (Target File 1), 421–434 (Target File 2)
* **Discrepancy Description**:  
  The design commits two classic architecture antipatterns:
  1. **Reinventing Existing Infrastructure**: Section 2 proposes an ad-hoc `std::vector<int> parent` Disjoint-Set implementation, ignoring the robust, tested `egraph::UnionFind` class in `src/core/union_find.h`.
  2. **Mischaracterizing Existing Types while Missing the Actual Defect**: Goal G5 and Section 6 frame `ScalarExpr` as a novel architectural capability. In reality, `struct ScalarExpr` already exists in `src/core/basic_types.h` (with expression trees, evaluation, and operators). Meanwhile, the design completely misses the actual breaking bug in the codebase: `using ScalarOperand = double;` in `src/core/op_registry.h:44` and `init_operands` converting `ScalarExpr` to `NaN`.
* **Rationale & Recommendation**:  
  Reinventing data structures fragments the codebase and inflates cognitive load. 
  
  **Recommendation**:  
  - If Union-Find is used anywhere, mandate `egraph::UnionFind` from `src/core/union_find.h`.
  - Reduce Section 6 to an incremental specification: upgrade `char var` to `std::string var` in `src/core/basic_types.h`.
  - Fix the true architectural defect in `src/core/op_registry.h`: change `using ScalarOperand = ScalarExpr;` (or `std::variant<double, ScalarExpr>`) and update `EvalFn` to accept `const DataBindings &data_bindings`.

---

### 4. Property Tag Taxonomy Divergence & Semantic Inconsistencies - Severity: 🟡 Major
* **Reviewer Lens:** `egm-zoom-out`
* **Affected Scope:** Lines 284–295 (Section 4: Canonical Property Tag Catalog), Lines 298–308
* **Discrepancy Description**:  
  1. **Taxonomy Clash**: The catalog invents abbreviated tags (`spd`, `fullrank`, `nonsingular`, `upper`, `lower`) that diverge from the established canonical names in `src/analysis/property_table.h` (`positive_definite`, `full_rank`, `non_singular`, `upper_triangular`, `lower_triangular`).
  2. **Bitmask vs Dynamic Predicate Conflation**: Section 4 claims all properties are evaluated via $O(1)$ bitwise checks. However, tags like `square` and `vector` are dynamic shape equality checks (`rows == cols`, `cols == 1`), not single bitmask flags on `MatrixFlags`.
* **Rationale & Recommendation**:  
  Divergent naming creates unnecessary translation layers and maintenance debt. 
  
  **Recommendation**:  
  Align property tag strings 1:1 with `MatrixProperty::flag_descriptors` in `src/analysis/property_table.h`. Clearly differentiate between bitwise matrix flags and shape-based dimension predicates in `Matcher::validate_property_constraints`.

---

### 5. In-Place Buffer Mutation Semantics for Absent Bias - Severity: 🟡 Major
* **Reviewer Lens:** `egm-zoom-out`
* **Affected Scope:** Lines 317–325 (Section 5: Table), Lines 370–376 (Section 7)
* **Discrepancy Description**:  
  `Op::Gemm` designates `in_place_input = 2` (operand $C$). In the proposed architecture, when bias is absent, an artificial zero matrix is synthesized for $C$. Setting `in_place_input` on a synthesized constant zero matrix implies in-place buffer mutation of a constant node, breaking memory planning invariants and alias analysis.
* **Rationale & Recommendation**:  
  In-place reuse is only valid when an existing accumulator buffer is provided by the expression graph ($A \cdot B + C$). When $\beta = 0.0$, the operation produces a fresh matrix buffer; it does not mutate an input.
  
  **Recommendation**:  
  Clarify in `OpDescriptor` that `in_place_input = 2` is conditional on $\beta \ne 0.0$ and $C$ being a mutable matrix operand, or handle buffer assignment cleanly in memory planning.

---

### 6. Missing Safe Error/Fallback Policies for Dynamic Scalar Bindings - Severity: 🟢 Nit
* **Reviewer Lens:** `egm-zoom-out`
* **Affected Scope:** Lines 348–349, 370–371, 485–486
* **Discrepancy Description**:  
  In `eval_gemm`, unassigned scalar parameters silently default via `.value_or(1.0)` or evaluate to `0.0` if missing from `DataBindings`. In numerical optimization pipelines, a missing step size parameter defaulting to `0.0` or `1.0` causes silent algorithm divergence or hard-to-debug numerical errors.
* **Rationale & Recommendation**:  
  Require `scalar_expr.evaluate(bindings)` to throw an explicit `std::runtime_error("Unbound scalar variable: ...")` when a required parameter is missing from `DataBindings`.

---

### 7. Test Target Proliferation & Verification Redundancy - Severity: 🟢 Nit
* **Reviewer Lens:** `egm-zoom-out`
* **Affected Scope:** Lines 503–526 (Target Files 11, 12, 13)
* **Discrepancy Description**:  
  The plan creates 3 separate new test targets. By decoupling shape deduction and simplifying lowering rules, pattern lowering can be verified entirely within `tests/unit/lowering_test.cpp`.
* **Rationale & Recommendation**:  
  Consolidate kernel lowering and property matching tests into `tests/unit/lowering_test.cpp`, eliminating redundant CMake test harnesses.

---

## Architectural Steering Roadmap: The "Pruned Core" Blueprint

The following table contrasts the over-engineered proposal with the recommended high-altitude pruned architecture:

| Dimension | Monolithic Proposal (Lines 1–555) | Pruned Core Architecture (Recommended) |
| :--- | :--- | :--- |
| **Primary Scope** | Shape Inference + Macro Expander + BLAS Lowering | **BLAS Kernel Lowering & Property Constraints Only** |
| **Target Files** | 14 files across entire compiler stack | **6 files** (`op_registry`, `evaluator`, `op_costs`, `lowering`, `matcher`, `basic_types`) |
| **Shape Deduction** | New `ShapeInferenceContext` + Disjoint-Set | **Deferred**; retain explicit `symbols` block |
| **Pattern Optionality** | Custom EBNF grammar + Macro AST + Expander | **Standard Rewrite rules**; eliminate pattern macro grammar |
| **Bias Lowering** | Procedural C++ applier synthesizing zero matrix | **Native CBLAS $\beta = 0.0$**; zero allocation overhead |
| **Scalar Representation** | "New" `ScalarExpr` leaf atoms | Incremental upgrade: `char var` $\implies$ `std::string var` |
| **CBLAS Dispatch** | String parsing hacks removed, but broken C++ types | Strongly typed `ScalarOperand = ScalarExpr`, `EvalFn` gets `data_bindings` |
| **Net Implementation Risk** | 🔴 Extreme (10 Blockers across Stages 5 & 6) | 🟢 Low (deterministic, incremental, high-value) |

### Minimal Target File Set (6 Files)

1. [`src/core/basic_types.h`](file:///Users/changguo/Projects/egraph-cpp/src/core/basic_types.h): Upgrade `char var` to `std::string var` in `ScalarExpr`.
2. [`src/core/op_registry.h`](file:///Users/changguo/Projects/egraph-cpp/src/core/op_registry.h): Register canonical unified ops (`Op::Gemm`, `Op::Syrk`, `Op::Trsm`, `Op::Trmm`, `Op::Symm`, `Op::Gemv`); update `ScalarOperand` to support `ScalarExpr`; update `EvalFn` to take `const DataBindings &`.
3. [`src/rewrite/matcher.h`](file:///Users/changguo/Projects/egraph-cpp/src/rewrite/matcher.h) & [`.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/rewrite/matcher.cpp): Implement $O(1)$ `validate_property_constraints` checking `MatrixProperty.flags`.
4. [`src/rule_sets/lowering.h`](file:///Users/changguo/Projects/egraph-cpp/src/rule_sets/lowering.h): Replace fragmented enum rules with unified rules using property constraints and native $\beta = 0.0$ scalar defaults.
5. [`src/evaluate/evaluator.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/evaluate/evaluator.cpp): Unified CBLAS dispatch routines reading strongly typed operands and evaluating `ScalarExpr`.
6. [`src/cost_functions/op_costs.cpp`](file:///Users/changguo/Projects/egraph-cpp/src/cost_functions/op_costs.cpp): Parameterized cost formulas for unified kernels.

---

## Action Items for Stage 9 (Spec Finalization)

1. **Excise Section 2 & Frontend Shape Inference**: Remove lines 186–229 and related grammar/directives from the spec. File a separate RFC for DSL shape deduction.
2. **Eliminate Synthetic Zero Matrices**: Rewrite Section 3 & Section 5 to define absent bias in terms of $\beta = 0.0$.
3. **Drop Macro Grammar Extensions**: Replace `[Tr](?a)` grammar extensions with standard rule definitions in `src/rule_sets/lowering.h`.
4. **Fix Type Definitions**: Update `ScalarOperand` in `op_registry.h` and specify `EvalFn` signature updates to cleanly resolve Comprehension Blocker 3 and Critic Blocker 4.
5. **Align Taxonomy**: Synchronize property tag names directly with `src/analysis/property_table.h`.
