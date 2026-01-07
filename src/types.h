#pragma once
#include <cstdint>
#include <variant>
#include <vector>
#include <string>

enum class Op
{
    Add,
    Mul,
    Transpose,
    Invert,
    Negate,
    Identity,
    Zero,
};

inline Op parse_op(std::string_view s)
{
    if (s == "Add")
        return Op::Add;
    if (s == "Mul")
        return Op::Mul;
    if (s == "Transpose")
        return Op::Transpose;
    if (s == "Invert")
        return Op::Invert;
    if (s == "Negate")
        return Op::Negate;
    if (s == "Identity")
        return Op::Identity;
    if (s == "Zero")
        return Op::Zero;
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

using Id = uint32_t;
using Children = std::vector<Id>;
using Atom = std::variant<Op, std::string>;
