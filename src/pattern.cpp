#include "pattern.h"

Pattern::Pattern(std::string_view s)
{
    s = trim(s);
    if (s.empty())
        throw std::runtime_error("Empty pattern");

    auto pos = s.find('(');
    if (pos == std::string_view::npos)
    {
        if (s == "Identity" || s == "Zero")
        {
            atom = parse_op(s);
            return;
        }
        else
        {
            atom = std::string(s);
            return;
        }
    }

    std::string_view op_str = trim(s.substr(0, pos));
    Op op = parse_op(op_str);

    size_t end = s.find_last_of(')');
    if (end == std::string_view::npos)
        throw std::runtime_error("Missing closing parenthesis");

    std::string_view args_str = s.substr(pos + 1, end - (pos + 1));
    std::vector<Pattern> children;

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
            children.emplace_back(args_str.substr(child_start, i - child_start));
            child_start = i + 1;
        }
    }
    if (child_start < args_str.size())
    {
        children.emplace_back(args_str.substr(child_start));
    }
    atom = op;
    this->children = std::move(children);
}