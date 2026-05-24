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
    QR,    // output: [Q, R]
    LU,    // output: [L, U, P]
    LLt,   // output: [L] where A = LLt
    Get,   // [tuple, index]
    Sol,   // [A, B] solving AX = B, output X
    Det,   // [A] computing det(A)
    Log,   // [A] computing log(A)
    Scale, // [A, int] representing scalar * A
    Gemm,  // Gemm(A, B, C) - General Matrix Multiply
    Syrk,  // Syrk(A, C) - Symmetric Rank-K update (always assume AA'+ C)
    Trsm,  // Trsm(A, B) - Triangular Solve Matrix (always assume AX = B)
    Potrf, // Cholesky factorization
    Geqrf, // QR factorization
    Gemv,  // Gemv(A, x, y) - General Matrix-Vector Multiply
};

using Id = size_t;
using Children = std::vector<Id>;
using Atom = std::variant<Op, std::string, int>; // int for indexes in Get operations
using Size = std::variant<int, std::string>;
using Shape = std::pair<Size, Size>;
using SizeBindings = std::unordered_map<std::string, int>;
