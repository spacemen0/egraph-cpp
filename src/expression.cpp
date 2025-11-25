#include "expression.h"

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

/// @brief parse a expression from a string with the format: Op(child1, child2, ...)
/// @param string
Expression::Expression(std::string_view string)
{
    auto pos = string.find('(');
    if (pos == std::string_view::npos)
    {
        op_or_string = std::string(trim(string));
        return;
    }

    if (auto op_str = std::string(trim(string.substr(0, pos))); op_str == "Add")
        op_or_string = Op::Add;
    else if (op_str == "Mul")
        op_or_string = Op::Mul;
    else if (op_str == "Transpose")
        op_or_string = Op::Transpose;
    else if (op_str == "Invert")
        op_or_string = Op::Invert;
    else if (op_str == "Negate")
        op_or_string = Op::Negate;
    else if (op_str == "Identity")
        op_or_string = Op::Identity;
    else if (op_str == "Zero")
        op_or_string = Op::Zero;
    else
        throw std::invalid_argument("Unknown operation: " + op_str);

    size_t start = pos + 1;
    size_t end = string.find_last_of(')');
    if (end == std::string_view::npos)
        throw std::invalid_argument("Unmatched parentheses in expression: " + std::string(string));

    size_t child_start = start;
    int paren_count = 0;
    for (size_t i = start; i < end; ++i)
    {
        if (string[i] == '(')
            paren_count++;
        else if (string[i] == ')')
            paren_count--;
        else if (string[i] == ',' && paren_count == 0)
        {
            if (auto child_str = (trim(string.substr(child_start, i - child_start))); !child_str.empty())
            {
                children.emplace_back(child_str);
            }
            child_start = i + 1;
        }
    }
    // Add the last child
    if (child_start < end)
        if (auto child_str = (trim(string.substr(child_start, end - child_start))); !child_str.empty())
        {
            children.emplace_back(child_str);
        }
}
