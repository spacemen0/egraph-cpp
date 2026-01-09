#pragma once

#include "types.h"
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>

struct ParsedAtom
{
    Atom atom;
    std::vector<std::string> children_strings;
};

inline ParsedAtom parse_string_to_atom(std::string_view s)
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
            auto child = trim(args_str.substr(child_start, i - child_start));
            if (!child.empty())
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
