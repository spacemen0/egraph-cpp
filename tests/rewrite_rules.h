#pragma once
#include "rewriter.h"
#include "pattern.h"
#include "e_graph.h"
#include "utils.h"

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