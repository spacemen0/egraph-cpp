#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <charconv>
#include <iostream>
#include "rewriter.h"
#include "types.h"
#include "errors.h"

inline Op parse_op(std::string_view s)
{
    using enum Op;
    if (s == "Add")
        return Add;
    if (s == "Mul")
        return Mul;
    if (s == "Transpose")
        return Transpose;
    if (s == "Invert")
        return Invert;
    if (s == "Negate")
        return Negate;
    if (s == "QR")
        return QR;
    if (s == "LU")
        return LU;
    if (s == "LLt")
        return LLt;
    if (s == "Get")
        return Get;
    if (s == "Solve")
        return Solve;
    if (s == "TriangularSolve")
        return TriangularSolve;
    if (s == "Determinant")
        return Determinant;
    if (s == "Log")
        return Log;
    throw InvalidOperationError("Unknown operation: " + std::string(s));
}

constexpr std::string_view trim(std::string_view sv)
{
    constexpr std::string_view WHITESPACE = " \n\r\t\f\v";
    auto start = sv.find_first_not_of(WHITESPACE);

    if (start == std::string_view::npos)
    {
        return {};
    }

    sv.remove_prefix(start);

    auto end = sv.find_last_not_of(WHITESPACE);

    sv.remove_suffix(sv.size() - (end + 1));

    return sv;
}

inline ParsedAtom string_to_parsed_atom(std::string_view s)
{
    s = trim(s);
    if (s.empty())
        throw ParseError("Empty string");

    auto pos = s.find('(');

    // Leaf node
    if (pos == std::string_view::npos)
    {
        int v;
        if (auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v); ec == std::errc())
        {
            if (ptr == s.data() + s.size()) // the entire string was consumed
                return {v, {}};
            throw ParseError("Invalid integer: " + std::string(s));
        }
        return {std::string(s), {}};
    }

    std::string_view op_str = trim(s.substr(0, pos));
    Op op = parse_op(op_str);

    size_t end = s.find_last_of(')');
    if (end == std::string_view::npos)
        throw ParseError("Missing closing parenthesis");

    std::string_view args_str = s.substr(pos + 1, end - (pos + 1));
    std::vector<std::string> children;

    size_t child_start = 0;
    int paren_count = 0;
    for (size_t i = 0; i < args_str.size(); ++i)
    {
        if (args_str[i] == '(')
            paren_count++;
        else if (args_str[i] == ')')
            paren_count--;
        else if (args_str[i] == ',' && paren_count == 0)
        {

            if (auto child = trim(args_str.substr(child_start, i - child_start)); !child.empty())
                children.emplace_back(child);
            child_start = i + 1;
        }
    }
    if (child_start < args_str.size())
    {
        auto child = trim(args_str.substr(child_start));
        if (!child.empty())
            children.emplace_back(child);
    }
    return {op, children};
}

inline std::string atom_to_string(const Atom &atom)
{
    if (std::holds_alternative<Op>(atom))
    {
        switch (std::get<Op>(atom))
        {
            using enum Op;
        case Add:
            return "Add";
        case Mul:
            return "Mul";
        case Transpose:
            return "Transpose";
        case Invert:
            return "Invert";
        case Negate:
            return "Negate";
        case QR:
            return "QR";
        case LU:
            return "LU";
        case LLt:
            return "LLt";
        case Get:
            return "Get";
        case Solve:
            return "Solve";
        case TriangularSolve:
            return "TriangularSolve";
        case Determinant:
            return "Determinant";
        case Log:
            return "Log";
        }
        return "UnknownOp";
    }
    else if (std::holds_alternative<std::string>(atom))
    {
        return std::get<std::string>(atom);
    }
    else if (std::holds_alternative<int>(atom))
    {
        return std::to_string(std::get<int>(atom));
    }
    return "UnknownAtom";
}

inline Rewrite make_rewrite(const std::string &name, std::string_view lhs, std::string_view rhs, const std::function<bool(const EGraph &, const Substitution &)> &condition = nullptr, const std::function<Id(EGraph &, const Substitution &)> &applier = nullptr)
{
    return Rewrite{name, Pattern(lhs), Pattern(rhs), condition, applier};
}

inline Id make_identity_for(EGraph &egraph, const Substitution &s, const std::string &var_name, bool use_first_dim = true)
{
    Id id = s.at(var_name);
    const auto &data = egraph.get_class_analysis_data(id);
    const auto *matrix_prop_ptr = std::get_if<MatrixProperty>(&data.property);
    if (!matrix_prop_ptr)
    {
        throw std::runtime_error("make_identity_for: Expected MatrixProperty but got TupleProperty");
    }
    const auto &matrix_prop = *matrix_prop_ptr;
    auto shape = matrix_prop.shape;

    MatrixProperty prop;
    prop.shape = use_first_dim ? std::make_pair(shape.first, shape.first) : std::make_pair(shape.second, shape.second);
    prop.is_identity = true;
    prop.is_symmetric = true;
    prop.is_orthogonal = true;
    prop.is_zero = false;

    prop.is_diagonal = true;
    prop.is_orthogonal = true;
    prop.is_singular = false;
    prop.is_upper_triangular = true;
    prop.is_lower_triangular = true;
    prop.is_symmetric = true;

    if (egraph.find_class_with_property(prop).has_value())
    {
        return egraph.find_class_with_property(prop).value();
    }

    std::string size_str;
    if (use_first_dim)
    {
        if (auto val = std::get_if<int>(&shape.first))
            size_str = std::to_string(*val);
        else
            size_str = std::get<std::string>(shape.first);
    }
    else
    {
        if (auto val = std::get_if<int>(&shape.second))
            size_str = std::to_string(*val);
        else
            size_str = std::get<std::string>(shape.second);
    }

    std::string identity_name = "I_" + size_str + "x" + size_str;
    egraph.register_property(identity_name, prop);
    return egraph.add_node(ENode({}, identity_name));
}

inline Id make_zero_for(EGraph &g, const Substitution &s, const std::string &var_name)
{
    Id id = s.at(var_name);
    const auto &data = g.get_class_analysis_data(id);
    const auto *matrix_prop_ptr = std::get_if<MatrixProperty>(&data.property);
    if (!matrix_prop_ptr)
    {
        throw std::runtime_error("make_zero_for: Expected MatrixProperty but got TupleProperty");
    }
    const auto &matrix_prop = *matrix_prop_ptr;
    auto shape = matrix_prop.shape;

    MatrixProperty prop;
    prop.shape = shape;
    prop.is_zero = true;
    prop.is_identity = false;
    prop.is_diagonal = true;
    prop.is_identity = false;
    prop.is_singular = true;
    prop.is_upper_triangular = true;
    prop.is_lower_triangular = true;
    prop.is_symmetric = true;

    if (g.find_class_with_property(prop).has_value())
    {
        return g.find_class_with_property(prop).value();
    }

    std::string h_str, w_str;
    if (auto val = std::get_if<int>(&shape.first))
        h_str = std::to_string(*val);
    else
        h_str = std::get<std::string>(shape.first);

    if (auto val = std::get_if<int>(&shape.second))
        w_str = std::to_string(*val);
    else
        w_str = std::get<std::string>(shape.second);

    std::string zero_name = "Zero_" + h_str + "x" + w_str;

    g.register_property(zero_name, prop);

    return g.add_node(ENode({}, zero_name));
}

inline bool is_identity(const Substitution &s, const EGraph &g, const std::string &var)
{
    Id id = s.at(var);
    if (auto prop = std::get_if<MatrixProperty>(&g.get_class_analysis_data(id).property))
        return prop->is_identity;
    return false;
}

inline bool is_zero(const Substitution &s, const EGraph &g, const std::string &var)
{
    Id id = s.at(var);
    if (auto prop = std::get_if<MatrixProperty>(&g.get_class_analysis_data(id).property))
        return prop->is_zero;
    return false;
}