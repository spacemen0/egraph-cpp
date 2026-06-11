#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

enum class Op {
    Add,
    Mul,
    Minus,
    Tr,
    Inv,
    QR,     // output: [Q, R] reduced QR
    LU,     // output: [L, U, P]
    LLt,    // output: [L] where A = LLt
    UtU,    // output: [U] where A = UtU (representing A = U^T U)
    Get,    // [tuple, index]
    Sol,    // [A, B] solving AX = B, output X
    SolR,   // [A, B] solving XA = B, output X
    Det,    // [A] computing det(A)
    Log,    // [A] computing log(A)
    Scale,  // [A, int] representing scalar * A
    Gemm,   // Gemm(A, B, C) - General Matrix Multiply (can pass Trans(A), Scale(A, 2), etc. as inputs)
    Syrk,   // Syrk(A, C) - Symmetric Rank-K update (always assume AA'+ C, can pass Trans(A) as input)
    Trsm,   // Trsm(A, B) - Triangular Solve Matrix (always assume AX = B, have rewrite rules to convert XA = B to this
            // with transpose)
    Potrf,  // Cholesky factorization
    Geqrf,  // QR factorization
    Trtri,  // Inverse of a triangular matrix
    Gemv,   // Gemv(A, x, y) - General Matrix-Vector Multiply
    Gemv_T, // Gemv(A, x, y) - General Matrix-Vector Multiply with transposed A
    Gemm_NN,
    Gemm_TN,
    Gemm_NT,
    Gemm_TT,
    Syrk_N,
    Syrk_T,
    Trsm_LN,
    Trsm_LT,
    Trsm_RN,
    Trsm_RT,
    Potrf_L,
    Potrf_U,
};

using Id = size_t;
using Children = std::vector<Id>;
using Atom = std::variant<Op, std::string, double>; // double for indexes in Get operations and scalars in Scale
using Size = std::variant<int, std::string>;
using Shape = std::pair<Size, Size>;
using SizeBindings = std::unordered_map<std::string, int>;

static inline bool is_kernel_op(Op op) {
    using enum Op;
    return op == Gemm || op == Syrk || op == Trsm || op == Potrf || op == Geqrf || op == Gemv || op == Gemv_T ||
           op == Trtri || op == Gemm_NN || op == Gemm_TN || op == Gemm_NT || op == Gemm_TT || op == Syrk_N ||
           op == Syrk_T || op == Trsm_LN || op == Trsm_LT || op == Trsm_RN || op == Trsm_RT || op == Potrf_L ||
           op == Potrf_U;
}
