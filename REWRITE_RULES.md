## Notes
Dynamic means the rhs of the rule can not simply be constructed using plain patterns and the rule has a applier which is a function that construct the new node (or modifies properties) with more complex logics.

Lots of rules theoretically can be deduced from more atomic rules existing I think but that don't necessarily mean they are redundant? 

## Definition of Operators
```cpp
enum class Op {
    Add,
    Mul,
    Minus,
    Tr,
    Inv,
    QR,       // output: [Q, R] reduced QR
    LU,       // output: [L, U, P]
    CholeL,   // output: [L] where A = CholeL
    CholeU,   // output: [U] where A = CholeU (representing A = U^T U)
    Get,      // [tuple, index]
    Scale,    // [A, int] representing scalar * A
    Geqrf,    // QR factorization
    Trtri,    // Inverse of a triangular matrix
    Gemv_N,   // Gemv(A, x, y) - General Matrix-Vector Multiply
    Gemv_T,   // Gemv(A, x, y) - General Matrix-Vector Multiply with transposed A
    Gemm_NN,  // Gemm(A, B, C) - General Matrix-Matrix Multiply
    Gemm_TN,  // Gemm(A, B, C) - General Matrix-Matrix Multiply with transposed A
    Gemm_NT,  // Gemm(A, B, C) - General Matrix-Matrix Multiply with transposed B
    Gemm_TT,  // Gemm(A, B, C) - General Matrix-Matrix Multiply with transposed A and B
    Syrk_N,   // Syrk(A, C) - Symmetric Rank-K Update
    Syrk_T,   // Syrk(A, C) - Symmetric Rank-K Update with transposed A
    Trmm_LN,  // Trmm(A, B, C) - Triangular Matrix Multiply, A is left, no transpose, C = AB + C
    Trmm_LT,  // Trmm(A, B, C) - Triangular Matrix Multiply, A is left, transposed, C = A^T B + C
    Trmm_RN,  // Trmm(A, B, C) - Triangular Matrix Multiply, A is right, no transpose, C = BA + C
    Trmm_RT,  // Trmm(A, B, C) - Triangular Matrix Multiply, A is right, transposed, C = BA^T + C
    Symm_L,   // Symm(A, B, C) - Symmetric Matrix Multiply, A is left, C = AB + C
    Symm_R,   // Symm(A, B, C) - Symmetric Matrix Multiply, A is right, C = BA + C
    Trsm_LN,  // Trsm(A, B) - Triangular Solve with multiple right-hand sides, AX = B, no transpose
    Trsm_LT,  // Trsm(A, B) - Triangular Solve with multiple right-hand sides, AX = B, transposed
    Trsm_RN,  // Trsm(A, B) - Triangular Solve with multiple right-hand sides, XA = B, no transpose
    Trsm_RT,  // Trsm(A, B) - Triangular Solve with multiple right-hand sides, XA = B, transposed
    Potrf_L,  // Potrf(A) - Cholesky factorization, lower triangular
    Potrf_U,  // Potrf(A) - Cholesky factorization, upper triangular
    Orgqr,    // Orgqr(A) - Generate explicit Q from implicit Householder reflectors
    Ormqr_LN, // Ormqr(A, B) - Multiply B by implicit Q from left, no transpose
    Ormqr_LT, // Ormqr(A, B) - Multiply B by implicit Q from left, transposed
    Ormqr_RN, // Ormqr(A, B) - Multiply B by implicit Q from right, no transpose
    Ormqr_RT, // Ormqr(A, B) - Multiply B by implicit Q from right, transposed
    Axpy,     // Axpy(x, y) - Addition (y = x + y)
};
```
---

## 1. Property Discovery Rules

### `transpose_spd`
- **Pattern:** `?a * Tr(?a)` -> `Dynamic`
- **Description:**: Marks $A A^T$ as symmetric and positive semi-definite, and positive definite if A has full row rank.

### `transpose_spd2`
- **Pattern:** `Tr(?a) * ?a` -> `Dynamic`
- **Description:**: Marks $A^T A$ as symmetric and positive semi-definite, and positive definite if A has full column rank.

### `sandwich_spd_left`
- **Pattern:** `Tr(?a) * (?b * ?a)` -> `Dynamic`
- **Description:**: Marks $A^T B A$ as symmetric when B is symmetric (and positive semi-definite if B is positive semi-definite), and positive definite if B is positive definite and A has full column rank.

### `sandwich_spd_left_assoc`
- **Pattern:** `(Tr(?a) * ?b) * ?a` -> `Dynamic`
- **Description:**: Associative variant of `sandwich_spd_left`.

### `sandwich_spd_right`
- **Pattern:** `?a * (?b * Tr(?a))` -> `Dynamic`
- **Description:**: Marks $A B A^T$ as symmetric when B is symmetric (and positive semi-definite if B is positive semi-definite), and positive definite if B is positive definite and A has full row rank.

### `sandwich_spd_right_assoc`
- **Pattern:** `(?a * ?b) * Tr(?a)` -> `Dynamic`
- **Description:**: Associative variant of `sandwich_spd_right`.

---

## 2. Simplification Rules

### `minus-cancel`
- **Pattern:** `?a - ?a` -> `0` (Dynamic)

### `add-comm-zero`
- **Pattern:** `?a + ?z` -> `?a`
- **Description:**: When z is zero matrix

### `mul-zero-left`
- **Pattern:** `?z * ?a` -> `0` (Dynamic)
- **Description:**: When z is zero matrix

### `mul-zero-right`
- **Pattern:** `?a * ?z` -> `0` (Dynamic)
- **Description:**: When z is zero matrix

### `scale_zero_scalar`
- **Pattern:** `Scale(?a, 0.0)` -> `0` (Dynamic)

### `scale_zero_matrix`
- **Pattern:** `Scale(?a, ?z)` -> `?a`
- **Description:**: when a is zero matrix

### `mul-identity-left`
- **Pattern:** `?a * ?i` -> `?a`
- **Description:**: when i is identity matrix

### `mul-identity-right`
- **Pattern:** `?i * ?a` -> `?a`
- **Description:**: when i is identity matrix

### `scale_one`
- **Pattern:** `Scale(?a, 1.0)` -> `?a`

### `tr-tr-cancel`
- **Pattern:** `Tr(Tr(?a))` -> `?a`

### `tr_symmetric`
- **Pattern:** `Tr(?a)` -> `?a`
- **Description:**: when a is symmetric

### `invert-cancel-left`
- **Pattern:** `Inv(?a) * ?a` -> `I` (Dynamic)

### `invert-cancel-right`
- **Pattern:** `?a * Inv(?a)` -> `I` (Dynamic)

### `scale-collapse`
- **Pattern:** `Scale(Scale(?a, ?s1), ?s2)` -> `Scale(?a, s1 * s2)` (Dynamic)
- **Description:**: when s1 and s2 are double data type

### `scale_combine`
- **Pattern:** `Scale(?a, ?s1) + Scale(?a, ?s2)` -> `Scale(?a, s1 + s2)` (Dynamic)
- **Description:**:  when s1 and s2 are double data type

### `scale_combine_implicit`
- **Pattern:** `Scale(?a, ?s1) + ?a` -> `Scale(?a, s1 + 1.0)` (Dynamic)
- **Description:**:  when s1 is double data type

### `orthogonal-transpose`
- **Pattern:** `Tr(?a) * ?a` -> `I` (Dynamic)
- **Description:**: when a is orthogonal 

### `orthonormal-transpose`
- **Pattern:** `Tr(?a) * ?a` -> `I` (Dynamic)
- **Description:**: when a is orthonormal

---

## 3. Transformation Rules

### `mul-assoc-left`
- **Pattern:** `?a * (?b * ?c)` <-> `(?a * ?b) * ?c` (bidirectional when I write "<->")

### `add-assoc`
- **Pattern:** `(?a + ?b) + ?c` <-> `?a + (?b + ?c)`

### `commute-add`
- **Pattern:** `?a + ?b` -> `?b + ?a`

### `mul-distribute-over-add-left`
- **Pattern:** `?a * (?b + ?c)` <-> `?a * ?b + ?a * ?c`

### `mul-distribute-over-add-right`
- **Pattern:** `(?a + ?b) * ?c` <-> `?a * ?c + ?b * ?c`

### `sub_to_add_scale`
- **Pattern:** `?a - ?b` -> `?a + Scale(?b, -1.0)`

(I think a lot of these scale rules are not used and tested)
### `scale_add_distribute`
- **Pattern:** `Scale(?a + ?b, ?s)` <-> `Scale(?a, ?s) + Scale(?b, ?s)`

### `scale_mul_distribute_left`
- **Pattern:** `Scale(?a, ?s) * ?b` -> `Scale(?a * ?b, ?s)` 

### `scale_mul_distribute_right`
- **Pattern:** `?a * Scale(?b, ?s)` -> `Scale(?a * ?b, ?s)`

### `invert-mat-prod`
- **Pattern:** `Inv(?a * ?b)` -> `Inv(?b) * Inv(?a)`
- **Description:**: a and b need to be invertible 

### `mat-transpose-prod`
- **Pattern:** `Tr(?a * ?b)` -> `Tr(?b) * Tr(?a)`

### `symm_prod_transpose_right`
- **Pattern:** `?a * ?b` -> `Tr(?b * Tr(?a))`
- **Description:**: when b is symmetric

### `symm_prod_transpose_left`
- **Pattern:** `?b * ?a` -> `Tr(Tr(?a) * ?b)`
- **Description:**: when b is symmetric

### `orthogonal-inverse`
- **Pattern:** `Inv(?a)` -> `Tr(?a)`
- **Description:**: when a is orthogonal

### `scale_transpose`
- **Pattern:** `Tr(Scale(?a, ?s))` <-> `Scale(Tr(?a), ?s)`

### `scale_inverse`
- **Pattern:** `Inv(Scale(?a, ?s))` <-> `Scale(Inv(?a), 1.0 / s)` (Dynamic)
- **Description:**: when s is double data type

---

## 4. Expansion Rules

### `qr-invert`
- **Pattern:** `Inv(?a)` -> `Inv(Get(QR(?a), 1)) * Tr(Get(QR(?a), 0))`
- **Description:**: Replaces $A^{-1}$ with $R^{-1} Q^T$

### `qr-leaf`
- **Pattern:** `?a` -> `Get(QR(?a), 0) * Get(QR(?a), 1)`
- **Description:**: Expands an unfactorized leaf matrix into its QR product $Q \cdot R$.

### `cholel-invert`
- **Pattern:** `Inv(?a)` -> `Tr(Inv(Get(CholeL(?a), 0))) * Inv(Get(CholeL(?a), 0))`
- **Description:**: Replaces $A^{-1}$ with $(L^{-1})^T L^{-1}$ for symmetric positive definite matrices using Cholesky factor $L$.

### `cholel-leaf`
- **Pattern:** `?a` -> `Get(CholeL(?a), 0) * Tr(Get(CholeL(?a), 0))`
- **Description:**: Expands an unfactorized symmetric positive definite leaf into Cholesky product $L L^T$.

### `cholel_to_choleu`
- **Pattern:** `Get(CholeL(?a), 0)` <-> `Tr(Get(CholeU(?a), 0))`
- **Description:**: Converts between lower and upper Cholesky factors bidirectionally 

### `lu-invert` *(inactive)*
- **Pattern:** `Inv(?a)` -> `Inv(Get(LU(?a), 1)) * Inv(Get(LU(?a), 0))`
- **Description:**: Replaces $A^{-1}$ with $U^{-1} L^{-1}$.

### `lu-leaf` *(inactive)*
- **Pattern:** `?a` -> `Get(LU(?a), 0) * Get(LU(?a), 1)`
- **Description:**: Expands an unfactorized square leaf into LU product.

---

## 5. Lowering Rules

### BLAS Level 1

#### `axpy`
- **Pattern:** `?a + ?b` -> `Axpy(?a, ?b)`
- **Description:**: Lowers vector or matrix addition into BLAS `axpy`.

#### `axpy_minus`
- **Pattern:** `?a - ?b` -> `Axpy(?a, Scale(?b, -1))`
- **Description:**: Lowers matrix or vector subtraction into BLAS `axpy`.

---

### BLAS Level 2

#### `gemv_without_c`
- **Pattern:** `?a * ?b` -> `Gemv_N(?a, ?b, 0)` (Dynamic)
- **Description:**: when a is matrix, b is vector and a is not a transpose node

#### `gemv_with_c`
- **Pattern:** `?a * ?b + ?c` -> `Gemv_N(?a, ?b, ?c)`
- **Description:**: when a is matrix, b is vector c is a vector and a is not a transpose node

#### `gemv_t_without_c`
- **Pattern:** `Tr(?a) * ?b` -> `Gemv_T(?a, ?b, 0)` (Dynamic)
- **Description:**: when a is matrix, b is vector

#### `gemv_t_with_c`
- **Pattern:** `Tr(?a) * ?b + ?c` -> `Gemv_T(?a, ?b, ?c)`
- **Description:**: when a is matrix, b is vector c is a vector
---

### BLAS Level 3: Matrix-Matrix Operations

#### `gemm_without_c`
- **Pattern:** `?a * ?b` -> `Gemm_NN(?a, ?b, 0)` (Dynamic)
- **Description:**: when b is not vector

#### `gemm_with_c`
- **Pattern:** `?a * ?b + ?c` -> `Gemm_NN(?a, ?b, ?c)`
- **Description:**: when b is not vector


#### `gemm_tn`
- **Pattern:** `Gemm_NN(Tr(?a), ?b, ?c)` -> `Gemm_TN(?a, ?b, ?c)`

#### `gemm_nt`
- **Pattern:** `Gemm_NN(?a, Tr(?b), ?c)` -> `Gemm_NT(?a, ?b, ?c)`

#### `gemm_tt`
- **Pattern:** `Gemm_NN(Tr(?a), Tr(?b), ?c)` -> `Gemm_TT(?a, ?b, ?c)`

#### `gemm_to_symm_l`
- **Pattern:** `Gemm_NN(?a, ?b, ?c)` -> `Symm_L(?a, ?b, ?c)`
- **Description:**: when a is symmetric

#### `gemm_to_symm_r`
- **Pattern:** `Gemm_NN(?b, ?a, ?c)` -> `Symm_R(?a, ?b, ?c)`
- **Description:**: when a is symmetric

#### `gemm_to_trmm_ln`
- **Pattern:** `Gemm_NN(?a, ?b, ?c)` -> `Trmm_LN(?a, ?b, ?c)`
- **Description:**: when a is triangular

#### `gemm_to_trmm_lt`
- **Pattern:** `Gemm_TN(?a, ?b, ?c)` -> `Trmm_LT(?a, ?b, ?c)`
- **Description:**: when a is triangular

#### `gemm_to_trmm_rn`
- **Pattern:** `Gemm_NN(?b, ?a, ?c)` -> `Trmm_RN(?a, ?b, ?c)`
- **Description:**:when a is triangular

#### `gemm_to_trmm_rt`
- **Pattern:** `Gemm_NT(?b, ?a, ?c)` -> `Trmm_RT(?a, ?b, ?c)`
- **Description:**:when a is triangular

#### `syrk_without_c_left`
- **Pattern:** `?a * Tr(?a)` -> `Syrk_N(?a, 0)` (Dynamic) 

#### `syrk_without_c_right`
- **Pattern:** `Tr(?a) * ?a` -> `Syrk_T(?a, 0)` (Dynamic)

#### `syrk_with_c_left`
- **Pattern:** `?a * Tr(?a) + ?c` -> `Syrk_N(?a, ?c)`

#### `syrk_with_c_right`
- **Pattern:** `Tr(?a) * ?a + ?c` -> `Syrk_T(?a, ?c)`

#### `syrk_t`
- **Pattern:** `Syrk_N(Tr(?a), ?c)` -> `Syrk_T(?a, ?c)`

#### `syrk_n`
- **Pattern:** `Syrk_T(Tr(?a), ?c)` -> `Syrk_N(?a, ?c)`

#### `trsm_ln`
- **Pattern:** `Inv(?a) * ?b` -> `Trsm_LN(?a, ?b)`
- **Description:**: when a is triangular

#### `trsm_lt_tr_inv`
- **Pattern:** `Tr(Inv(?a)) * ?b` -> `Trsm_LT(?a, ?b)`
- **Description:**: when a is triangular

#### `trsm_lt_inv_tr`
- **Pattern:** `Inv(Tr(?a)) * ?b` -> `Trsm_LT(?a, ?b)`
- **Description:**:  when a is triangular

#### `trsm_lt`
- **Pattern:** `Trsm_LN(Tr(?a), ?b)` -> `Trsm_LT(?a, ?b)`

#### `trsm_rn`
- **Pattern:** `?b * Inv(?a)` -> `Trsm_RN(?a, ?b)`
- **Description:**: when a is triangular

#### `trsm_rt_tr_inv`
- **Pattern:** `?b * Tr(Inv(?a))` -> `Trsm_RT(?a, ?b)`
- **Description:**:  when a is triangular

#### `trsm_rt_inv_tr`
- **Pattern:** `?b * Inv(Tr(?a))` -> `Trsm_RT(?a, ?b)`
- **Description:**:  when a is triangular

#### `trsm_rt`
- **Pattern:** `Trsm_RN(Tr(?a), ?b)` -> `Trsm_RT(?a, ?b)`

---

### LAPACK Routines

#### `potrf_l`
- **Pattern:** `CholeL(?a)` -> `Potrf_L(?a)`

#### `potrf_u`
- **Pattern:** `CholeU(?a)` -> `Potrf_U(?a)`

#### `potrf_u_to_potrf_l`
- **Pattern:** `Get(Potrf_U(?a), 0)` -> `Tr(Get(Potrf_L(?a), 0))`

#### `potrf_l_to_potrf_u`
- **Pattern:** `Get(Potrf_L(?a), 0)` -> `Tr(Get(Potrf_U(?a), 0))`

#### `geqrf`
- **Pattern:** `QR(?a)` -> `Geqrf(?a)`

#### `get_orgqr`
- **Pattern:** `Get(Geqrf(?a), 0)` -> `Orgqr(Geqrf(?a))`
- **Description:**: Generates explicit orthogonal matrix Q. 

#### `fuse_ormqr_ln`
- **Pattern:** `Get(Geqrf(?a), 0) * ?b` -> `Ormqr_LN(Geqrf(?a), ?b)`

#### `fuse_ormqr_lt`
- **Pattern:** `Tr(Get(Geqrf(?a), 0)) * ?b` -> `Ormqr_LT(Geqrf(?a), ?b)`

#### `fuse_ormqr_rn`
- **Pattern:** `?b * Get(Geqrf(?a), 0)` -> `Ormqr_RN(Geqrf(?a), ?b)`

#### `fuse_ormqr_rt`
- **Pattern:** `?b * Tr(Get(Geqrf(?a), 0))` -> `Ormqr_RT(Geqrf(?a), ?b)`

#### `trtri`
- **Pattern:** `Inv(?a)` -> `Trtri(?a)`
- **Description:**: when a is triangular
