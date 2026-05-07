---
marp: true
theme: gaia
paginate: true
backgroundColor: #fff
footer: E-Graphs for Linear Algebra optimizer
style: |
  section {
    font-size: 28px;
  }
math: mathjax
---

# A Non-Destructive, Shape-Agnostic Optimizer for Linear Algebra



**By**: Chang Guo
**Main Advisor**: Paolo Bientinesi
**Co-Advisor**: Lars Kalsson

---

## The Landscape of Computational Linear Algebra

* **The Gold Standard:** BLAS and LAPACK becomes the standard low-level computation kernels for their near-optimal performance. 
  - Written in Fortran
  - Language bindings exist
  - Constructing call chains is tedious and error-prone.

* **The Abstraction:** High-level tools like **Matlab, Julia, Eigen, and Armadillo** have gained popularity by allowing mathematical expressions to be written naturally.

* **The Cost of Abstraction:** The internal mapping algorithm often results in suboptimal execution code.
  - One reason being that they rarely exploit the **properties** of the matrices (e.g., symmetry, positive definiteness).

---

## The Problem Defined and the Approach

**Given:** A mathematical expression (e.g., `Inv(Tr(X) * X) * Tr(X) * y`), and the properties of the matrices involved.

**Goal:** Generate efficient BLAS/LAPACK call sequences.

**Solution:** We build an E-Graph to store the vast space of mathematically valid variations of the input expression. We map the symbolic mathematical operators to BLAS/LAPACK calls. When the actual matrix sizes are known, we can extract the best-performing plan based on the cost model.

---

<!-- ## The Approach: The E-Graph Successor

Instead of searching for *one* best plan, we store **every possible plan** in an **E-Graph**.

1. **Decoupled Search:** Mathematical identities are applied until the graph is "saturated."
2. **Late-Binding:** We don't need to know the matrix sizes to optimize.
3. **Symbolic Costing:** We cost every path using monomials (e.g., $2N^2M$).
4. **Instant Extraction:** When sizes are finally known, we "extract" the best plan in milliseconds.

--- -->

## E-Graph 101

An **Equivalence Graph (E-Graph)** is a data structure to compactly represent many ways to write the same expression.

- **E-Nodes:** Represent one way to write the expression.
- **E-Classes:** Sets of E-Nodes that are mathematically equivalent.

**Non-Destructive:** We never replace $A(BC)$ with $(AB)C$. We store **both** in the same e-class. The graph only grows; it never loses a potentially better path. Unless we manually prune it.

---

## Example of E-Graphs:
![alt text](image.png)
---

## Case Study: Ordinary Least Squares (OLS)

Expression: `(Inv(Tr(X) * X) * Tr(X)) * y`

Our E-Graph saturates to hold:
- **Direct Inverse path:** Traditional, numerically unstable.
- **Cholesky path:** Valid if $X^T X$ is Positive Definiteness.
- **QR Decomposition path:** Faster and more stable for tall matrices.

**The "Glimpse":** For this single input, our e-graph holds **38,000+** mathematically valid variations simultaneously.

---

## Technical Challenge: Managing Explosion

PhD students will ask: *"Doesn't the graph grow infinitely?"*

We implement critical safeguards:
1. **Cycle Detection:** Preventing infinite expansions like $A \rightarrow \text{Tr}(\text{Tr}(A))$.
2. **Greedy Pruning:** Using size-bindings to sample the graph and prune branches that are mathematically sound but computationally "garbage" across all plausible sizes.

---

## Current Status & Future Work

- [x] Functional C++ E-Graph Engine
- [x] Symbolic Monomial Cost Model
- [x] Non-destructive Rewriting (Associativity, Identities, Decompositions)
- [ ] **Next Step:** Integrating a code generator (C++/BLAS/Eigen).
- [ ] **Research Goal:** Auto-tuning the pruning heuristics using Machine Learning.

---

## Summary

We are moving away from **static optimization** toward **structural search libraries**.

1. **Exhaustive:** We find paths compile-time optimizers miss.
2. **Flexible:** One E-Graph serves all matrix sizes.
3. **Fast:** Extraction is $O(N)$ once the graph is built.

**Questions?**
*Check out the repo: egraph-cpp*
