#pragma once
#include "rewrite.h"
#include "pattern.h"
#include "types.h"
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <iostream>

inline Pattern parse_pattern(std::string_view s)
{
    s = trim(s);
    if (s.empty())
        throw std::runtime_error("Empty pattern");

    auto pos = s.find('(');
    if (pos == std::string_view::npos)
    {
        if (s == "Identity" || s == "Zero")
        {
            return Pattern{PatternAtom(parse_op(s)), {}};
        }
        else
        {
            return Pattern{PatternAtom(PatternVar{std::string(s)}), {}};
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
            children.push_back(parse_pattern(args_str.substr(child_start, i - child_start)));
            child_start = i + 1;
        }
    }
    if (child_start < args_str.size())
    {
        children.push_back(parse_pattern(args_str.substr(child_start)));
    }

    return Pattern{PatternAtom(op), children};
}

inline Rewrite make_rewrite(std::string name, std::string_view lhs, std::string_view rhs)
{
    return Rewrite{name, parse_pattern(lhs), parse_pattern(rhs)};
}

inline std::vector<Rewrite> get_all_rewrite_rules()
{
    return {
        make_rewrite("commute-add", "Add(a, b)", "Add(b, a)"),
        make_rewrite("assoc-add-left", "Add(a, Add(b, c))", "Add(Add(a, b), c)"),
        make_rewrite("assoc-add-right", "Add(Add(a, b), c)", "Add(a, Add(b, c))"),
        make_rewrite("mul-assoc", "Mul(a, Mul(b, c))", "Mul(Mul(a, b), c)"),
        make_rewrite("mul-assoc-right", "Mul(Mul(a, b), c)", "Mul(a, Mul(b, c))"),
        make_rewrite("mul-distrib", "Mul(a, Add(b, c))", "Add(Mul(a, b), Mul(a, c))"),
        make_rewrite("mul-distrib-left-right", "Add(Mul(a, b), Mul(a, c))", "Mul(a, Add(b, c))"),
        make_rewrite("mul-distrib-right", "Mul(Add(b, c), a)", "Add(Mul(b, a), Mul(c, a))"),
        make_rewrite("mul-distrib-right-right", "Add(Mul(b, a), Mul(c, a))", "Mul(Add(b, c), a)"),
        make_rewrite("mul-identity", "Mul(a, Identity)", "a"),
        make_rewrite("mul-identity-right", "Mul(Identity, a)", "a"),
        make_rewrite("mat-transpose-prod", "Transpose(Mul(a, b))", "Mul(Transpose(b), Transpose(a))"),
        make_rewrite("mat-transpose-prod-right", "Mul(Transpose(b), Transpose(a))", "Transpose(Mul(a, b))"),
        make_rewrite("transpose-involutive", "Transpose(Transpose(a))", "a"),
        make_rewrite("transpose-involutive-right", "a", "Transpose(Transpose(a))"),
        make_rewrite("invert-involutive", "Invert(Invert(a))", "a"),
        make_rewrite("invert-involutive-right", "a", "Invert(Invert(a))"),
        make_rewrite("invert-mat-prod", "Invert(Mul(a, b))", "Mul(Invert(b), Invert(a))"),
        make_rewrite("invert-mat-prod-right", "Mul(Invert(b), Invert(a))", "Invert(Mul(a, b))"),
        make_rewrite("invert-cancel-left", "Mul(Invert(a), a)", "Identity"),
        make_rewrite("invert-cancel-right", "Mul(a, Invert(a))", "Identity"),
        make_rewrite("add-comm-zero", "Add(a, Zero)", "a"),
        make_rewrite("mul-zero-left", "Mul(Zero, a)", "Zero"),
        make_rewrite("mul-zero-right", "Mul(a, Zero)", "Zero"),
        make_rewrite("negate-involutive", "Negate(Negate(a))", "a"),
        make_rewrite("negate-involutive-right", "a", "Negate(Negate(a))"),
        make_rewrite("add-negate-cancel-left", "Add(a, Negate(a))", "Zero"),
        make_rewrite("transpose-invert", "Transpose(Invert(a))", "Invert(Transpose(a))"),
        make_rewrite("invert-transpose", "Invert(Transpose(a))", "Transpose(Invert(a))"),
        make_rewrite("transpose-add", "Transpose(Add(a, b))", "Add(Transpose(a), Transpose(b))"),
        make_rewrite("transpose-add-right", "Add(Transpose(a), Transpose(b))", "Transpose(Add(a, b))"),
    };
}

static auto invert_cancel_left = make_rewrite("invert-cancel-left", "Mul(Invert(a), a)", "Identity");
static auto mul_assoc_right = make_rewrite("mul-assoc-right", "Mul(Mul(a, b), c)", "Mul(a, Mul(b, c))");
static auto mul_identity_right = make_rewrite("mul-identity-right", "Mul(Identity, a)", "a");