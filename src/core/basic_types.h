#pragma once

#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <variant>
#include <vector>

namespace egraph {
enum class Op : std::uint32_t {};

using SizeBindings = std::unordered_map<std::string, int>;
using DataBindings = std::unordered_map<std::string, std::vector<double>>;

enum class ScalarOp : std::uint8_t { Value, Var, Add, Sub, Mul, Div, Neg };

struct ScalarExpr {
    ScalarOp op = ScalarOp::Value;
    double val = 0.0;
    char var = '\0';
    std::vector<ScalarExpr> children;

    ScalarExpr() = default;
    // NOLINTNEXTLINE(misc-explicit-constructor)
    ScalarExpr(double v) : val(v) {}
    // NOLINTNEXTLINE(misc-explicit-constructor)
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
            // NOLINTNEXTLINE(modernize-return-braced-init-list) - braced init would call initializer_list<char>{1, var}
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

bool is_kernel_op(Op op);

} // namespace egraph
