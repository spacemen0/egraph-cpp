#include "expression.h"
#include <stdexcept>

/// @brief parse a expression from a string with the format: Op(child1, child2, ...)
/// @param string
Expression::Expression(std::string_view string)
{
    auto pos = string.find('(');
    if (pos == std::string_view::npos)
    {
        std::string trimmed = std::string(trim(string));
        if (trimmed == "Zero")
            atom = Op::Zero;
        else if (trimmed == "Identity")
            atom = Op::Identity;
        else
            atom = trimmed;
        return;
    }

    std::string_view op_str = trim(string.substr(0, pos));
    atom = parse_op(op_str);

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
