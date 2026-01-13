#pragma once
#include "types.h"
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>

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
    if (s == "Identity")
        return Identity;
    if (s == "Zero")
        return Zero;
    throw std::runtime_error("Unknown op: " + std::string(s));
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
        throw std::runtime_error("Empty string");

    auto pos = s.find('(');

    // Leaf node
    if (pos == std::string_view::npos)
    {
        if (s == "Identity")
            return {Op::Identity, {}};
        if (s == "Zero")
            return {Op::Zero, {}};

        return {std::string(s), {}};
    }

    std::string_view op_str = trim(s.substr(0, pos));
    Op op = parse_op(op_str);

    size_t end = s.find_last_of(')');
    if (end == std::string_view::npos)
        throw std::runtime_error("Missing closing parenthesis");

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
        case Identity:
            return "Identity";
        case Zero:
            return "Zero";
        }
        return "UnknownOp";
    }
    return std::get<std::string>(atom);
}