#pragma once

#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <variant>
#include <vector>

enum class Op {
    Add,
    Mul,
    Minus,
    Div,
    Tr,
    Inv,
    QR,       // output: [Q, R] reduced QR
    LU,       // output: [L, U, P]
    LLt,      // output: [L] where A = LLt
    UtU,      // output: [U] where A = UtU (representing A = U^T U)
    Get,      // [tuple, index]
    Sol,      // [A, B] solving AX = B, output X
    SolR,     // [A, B] solving XA = B, output X
    Det,      // [A] computing det(A)
    Log,      // [A] computing log(A)
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

using SizeBindings = std::unordered_map<std::string, int>;
using DataBindings = std::unordered_map<std::string, std::vector<double>>;

enum class ScalarOp { Value, Var, Add, Sub, Mul, Div, Neg };

struct ScalarExpr {
    ScalarOp op = ScalarOp::Value;
    double val = 0.0;
    char var = '\0';
    std::vector<ScalarExpr> children;

    ScalarExpr() = default;
    ScalarExpr(double v) : op(ScalarOp::Value), val(v) {}
    ScalarExpr(char c) : op(ScalarOp::Var), var(c) {}
    ScalarExpr(ScalarOp op, std::vector<ScalarExpr> children) : op(op), children(std::move(children)) {}

    bool operator==(const ScalarExpr &other) const {
        return op == other.op && val == other.val && var == other.var && children == other.children;
    }

    // Ordering
    bool operator<(const ScalarExpr &other) const {
        if (op != other.op)
            return op < other.op;
        if (val != other.val)
            return val < other.val;
        if (var != other.var)
            return var < other.var;
        return children < other.children;
    }

    double evaluate(const DataBindings &bindings = {}) const {
        switch (op) {
        case ScalarOp::Value:
            return val;
        case ScalarOp::Var: {
            std::string s(1, var);
            auto it = bindings.find(s);
            return (it != bindings.end() && !it->second.empty()) ? it->second[0] : 0.0;
        }
        case ScalarOp::Add:
            return children[0].evaluate(bindings) + children[1].evaluate(bindings);
        case ScalarOp::Sub:
            return children[0].evaluate(bindings) - children[1].evaluate(bindings);
        case ScalarOp::Mul:
            return children[0].evaluate(bindings) * children[1].evaluate(bindings);
        case ScalarOp::Div:
            return children[0].evaluate(bindings) / children[1].evaluate(bindings);
        case ScalarOp::Neg:
            return -children[0].evaluate(bindings);
        }
        return 0.0;
    }

    std::string to_string() const {
        switch (op) {
        case ScalarOp::Value: {
            std::string s = std::to_string(val);
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.')
                s.pop_back();
            return s;
        }
        case ScalarOp::Var:
            return std::string(1, var);
        case ScalarOp::Add:
            return "(" + children[0].to_string() + " + " + children[1].to_string() + ")";
        case ScalarOp::Sub:
            return "(" + children[0].to_string() + " - " + children[1].to_string() + ")";
        case ScalarOp::Mul:
            return "(" + children[0].to_string() + " * " + children[1].to_string() + ")";
        case ScalarOp::Div:
            return "(" + children[0].to_string() + " / " + children[1].to_string() + ")";
        case ScalarOp::Neg:
            return "-" + children[0].to_string();
        }
        return "";
    }
};

inline ScalarExpr operator+(const ScalarExpr &lhs, const ScalarExpr &rhs) { return {ScalarOp::Add, {lhs, rhs}}; }
inline ScalarExpr operator+(const ScalarExpr &lhs, double rhs) { return {ScalarOp::Add, {lhs, ScalarExpr(rhs)}}; }
inline ScalarExpr operator+(double lhs, const ScalarExpr &rhs) { return {ScalarOp::Add, {ScalarExpr(lhs), rhs}}; }
inline ScalarExpr operator-(const ScalarExpr &lhs, const ScalarExpr &rhs) { return {ScalarOp::Sub, {lhs, rhs}}; }
inline ScalarExpr operator-(const ScalarExpr &lhs, double rhs) { return {ScalarOp::Sub, {lhs, ScalarExpr(rhs)}}; }
inline ScalarExpr operator-(double lhs, const ScalarExpr &rhs) { return {ScalarOp::Sub, {ScalarExpr(lhs), rhs}}; }
inline ScalarExpr operator*(const ScalarExpr &lhs, const ScalarExpr &rhs) { return {ScalarOp::Mul, {lhs, rhs}}; }
inline ScalarExpr operator*(const ScalarExpr &lhs, double rhs) { return {ScalarOp::Mul, {lhs, ScalarExpr(rhs)}}; }
inline ScalarExpr operator*(double lhs, const ScalarExpr &rhs) { return {ScalarOp::Mul, {ScalarExpr(lhs), rhs}}; }
inline ScalarExpr operator/(const ScalarExpr &lhs, const ScalarExpr &rhs) { return {ScalarOp::Div, {lhs, rhs}}; }
inline ScalarExpr operator/(const ScalarExpr &lhs, double rhs) { return {ScalarOp::Div, {lhs, ScalarExpr(rhs)}}; }
inline ScalarExpr operator/(double lhs, const ScalarExpr &rhs) { return {ScalarOp::Div, {ScalarExpr(lhs), rhs}}; }
inline ScalarExpr operator-(const ScalarExpr &s) { return {ScalarOp::Neg, {s}}; }

using Id = size_t;
using Children = std::vector<Id>;
using LookupTable = std::unordered_map<std::string, uint32_t>; // maps string to unique integer id for storage in ENode
using Atom = std::variant<Op, uint32_t, int, ScalarExpr>;      // int for indexes in Get operations
using Size = std::variant<int, std::string>;
using Shape = std::pair<Size, Size>;

static inline bool is_kernel_op(Op op) {
    using enum Op;
    return op == Geqrf || op == Gemv_N || op == Gemv_T || op == Trtri || op == Gemm_NN || op == Gemm_TN ||
           op == Gemm_NT || op == Gemm_TT || op == Syrk_N || op == Syrk_T || op == Trsm_LN || op == Trsm_LT ||
           op == Trsm_RN || op == Trsm_RT || op == Potrf_L || op == Potrf_U || op == Orgqr || op == Ormqr_LN ||
           op == Ormqr_LT || op == Ormqr_RN || op == Ormqr_RT || op == Axpy;
}
