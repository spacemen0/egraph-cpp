#pragma once
#include "rewriter.h"
#include "pattern.h"
#include "types.h"
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <iostream>

inline Rewrite make_rewrite(const std::string &name, std::string_view lhs, std::string_view rhs, const std::function<bool(const Substitution &, const EGraph &)> &condition = nullptr)
{
    return Rewrite{name, Pattern(lhs), Pattern(rhs), condition};
}

inline Rewrite make_dynamic_rewrite(const std::string &name, std::string_view lhs, std::function<Id(EGraph &, const Substitution &)> applier)
{
    return Rewrite{name, Pattern(lhs), Pattern("?__dynamic__"), nullptr, applier};
}

inline Id make_identity_for(EGraph &egraph, const Substitution &s, std::string var_name)
{
    Id id = s.at(var_name);
    const auto &data = egraph.get_class_analysis_data(id);
    auto shape = data.property.shape;

    std::string h_str, w_str;
    if (auto val = std::get_if<int>(&shape.first))
        h_str = std::to_string(*val);
    else
        h_str = std::get<std::string>(shape.first);

    if (auto val = std::get_if<int>(&shape.second))
        w_str = std::to_string(*val);
    else
        w_str = std::get<std::string>(shape.second);

    std::string identity_name = "I_" + h_str + "x" + w_str;

    MatrixProperty prop;
    prop.shape = shape;
    prop.is_identity = true;
    prop.is_symmetric = true;
    prop.is_orthogonal = true;
    prop.is_zero = false;

    egraph.register_property(identity_name, prop);

    return egraph.add_node(ENode({}, identity_name));
}

inline Id make_zero_for(EGraph &g, const Substitution &s, std::string var_name)
{
    Id id = s.at(var_name);
    const auto &data = g.get_class_analysis_data(id);
    auto shape = data.property.shape;

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

    MatrixProperty prop;
    prop.shape = shape;
    prop.is_zero = true;
    prop.is_symmetric = true;
    prop.is_identity = false;

    g.register_property(zero_name, prop);

    return g.add_node(ENode({}, zero_name));
}

inline bool is_identity_prop(const Substitution &s, const EGraph &g, std::string var)
{
    Id id = s.at(var);
    return g.get_class_analysis_data(id).property.is_identity;
}

inline bool is_zero_prop(const Substitution &s, const EGraph &g, std::string var)
{
    Id id = s.at(var);
    return g.get_class_analysis_data(id).property.is_zero;
}

inline std::vector<Rewrite> get_all_rewrite_rules()
{
    return {
        make_rewrite("commute-add", "Add(?a, ?b)", "Add(?b, ?a)"),
        make_rewrite("assoc-add-left", "Add(?a, Add(?b, ?c))", "Add(Add(?a, ?b), ?c)"),
        make_rewrite("assoc-add-right", "Add(Add(?a, ?b), ?c)", "Add(?a, Add(?b, ?c))"),
        make_rewrite("mul-assoc", "Mul(?a, Mul(?b, ?c))", "Mul(Mul(?a, ?b), ?c)"),
        make_rewrite("mul-assoc-right", "Mul(Mul(?a, ?b), ?c)", "Mul(?a, Mul(?b, ?c))"),
        make_rewrite("mul-distrib", "Mul(?a, Add(?b, ?c))", "Add(Mul(?a, ?b), Mul(?a, ?c))"),
        make_rewrite("mul-distrib-left-right", "Add(Mul(?a, ?b), Mul(?a, ?c))", "Mul(?a, Add(?b, ?c))"),
        make_rewrite("mul-distrib-right", "Mul(Add(?b, ?c), ?a)", "Add(Mul(?b, ?a), Mul(?c, ?a))"),
        make_rewrite("mul-distrib-right-right", "Add(Mul(?b, ?a), Mul(?c, ?a))", "Mul(Add(?b, ?c), ?a)"),

        make_rewrite("mul-identity", "Mul(?a, ?i)", "?a", [](const Substitution &s, const EGraph &g)
                     { return is_identity_prop(s, g, "i"); }),
        make_rewrite("mul-identity-right", "Mul(?i, ?a)", "?a", [](const Substitution &s, const EGraph &g)
                     { return is_identity_prop(s, g, "i"); }),

        make_rewrite("mat-transpose-prod", "Transpose(Mul(?a, ?b))", "Mul(Transpose(?b), Transpose(?a))"),
        make_rewrite("mat-transpose-prod-right", "Mul(Transpose(?b), Transpose(?a))", "Transpose(Mul(?a, ?b))"),
        make_rewrite("transpose-involutive", "Transpose(Transpose(?a))", "?a"),
        make_rewrite("transpose-involutive-right", "?a", "Transpose(Transpose(?a))"),
        make_rewrite("invert-involutive", "Invert(Invert(?a))", "?a"),
        make_rewrite("invert-involutive-right", "?a", "Invert(Invert(?a))"),
        make_rewrite("invert-mat-prod", "Invert(Mul(?a, ?b))", "Mul(Invert(?b), Invert(?a))"),
        make_rewrite("invert-mat-prod-right", "Mul(Invert(?b), Invert(?a))", "Invert(Mul(?a, ?b))"),

        make_dynamic_rewrite("invert-cancel-left", "Mul(Invert(?a), ?a)", [](EGraph &g, const Substitution &s)
                             { return make_identity_for(g, s, "a"); }),
        make_dynamic_rewrite("invert-cancel-right", "Mul(?a, Invert(?a))", [](EGraph &g, const Substitution &s)
                             { return make_identity_for(g, s, "a"); }),

        make_rewrite("add-comm-zero", "Add(?a, ?z)", "?a", [](const Substitution &s, const EGraph &g)
                     { return is_zero_prop(s, g, "z"); }),
        make_rewrite("mul-zero-left", "Mul(?z, ?a)", "?z", [](const Substitution &s, const EGraph &g)
                     { return is_zero_prop(s, g, "z"); }),
        make_rewrite("mul-zero-right", "Mul(?a, ?z)", "?z", [](const Substitution &s, const EGraph &g)
                     { return is_zero_prop(s, g, "z"); }),

        make_rewrite("negate-involutive", "Negate(Negate(?a))", "?a"),
        make_rewrite("negate-involutive-right", "?a", "Negate(Negate(?a))"),

        make_dynamic_rewrite("add-negate-cancel-left", "Add(?a, Negate(?a))", [](EGraph &g, const Substitution &s)
                             { return make_zero_for(g, s, "a"); }),

        make_rewrite("transpose-invert", "Transpose(Invert(?a))", "Invert(Transpose(?a))"),
        make_rewrite("invert-transpose", "Invert(Transpose(?a))", "Transpose(Invert(?a))"),
        make_rewrite("transpose-add", "Transpose(Add(?a, ?b))", "Add(Transpose(?a), Transpose(?b))"),
        make_rewrite("transpose-add-right", "Add(Transpose(?a), Transpose(?b))", "Transpose(Add(?a, ?b))"),
    };
}

static const auto invert_cancel_left = make_dynamic_rewrite("invert-cancel-left", "Mul(Invert(?a), ?a)", [](EGraph &g, const Substitution &s)
                                                            { return make_identity_for(g, s, "a"); });
static const auto invert_cancel_right = make_dynamic_rewrite("invert-cancel-right", "Mul(?a, Invert(?a))", [](EGraph &g, const Substitution &s)
                                                             { return make_identity_for(g, s, "a"); });
static const auto mul_assoc_right = make_rewrite("mul-assoc-right", "Mul(Mul(?a, ?b), ?c)", "Mul(?a, Mul(?b, ?c))");
static const auto mul_identity_right = make_rewrite("mul-identity-right", "Mul(?i, ?a)", "?a", [](const Substitution &s, const EGraph &g)
                                                    { return is_identity_prop(s, g, "i"); });
static const auto mul_identity = make_rewrite("mul-identity", "Mul(?a, ?i)", "?a", [](const Substitution &s, const EGraph &g)
                                              { return is_identity_prop(s, g, "i"); });
static const auto mul_assoc = make_rewrite("mul-assoc", "Mul(?a, Mul(?b, ?c))", "Mul(Mul(?a, ?b), ?c)");
static const auto commute_add = make_rewrite("commute-add", "Add(?a, ?b)", "Add(?b, ?a)");
static const auto mat_transpose_prod = make_rewrite("mat-transpose-prod", "Transpose(Mul(?a, ?b))", "Mul(Transpose(?b), Transpose(?a))");