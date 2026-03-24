#pragma once
#include "e_graph.h"
#include "pattern.h"
#include "rewriter.h"
#include "utils.h"

inline std::vector<Rewrite> get_all_rewrite_rules() {
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

        make_rewrite(
            "mul-identity", "Mul(?a, ?i)", "?a",
            [](const EGraph &g, const Substitution &s) {
        return is_identity(s, g, "i");
    }),
        make_rewrite(
            "mul-identity-right", "Mul(?i, ?a)", "?a",
            [](const EGraph &g, const Substitution &s) {
        return is_identity(s, g, "i");
    }),

        make_rewrite("mat-transpose-prod", "Tr(Mul(?a, ?b))", "Mul(Tr(?b), Tr(?a))"),
        make_rewrite("mat-transpose-prod-right", "Mul(Tr(?b), Tr(?a))", "Tr(Mul(?a, ?b))"),
        make_rewrite("transpose-involutive", "Tr(Tr(?a))", "?a"),
        make_rewrite("transpose-involutive-right", "?a", "Tr(Tr(?a))"),
        make_rewrite("invert-involutive", "Inv(Inv(?a))", "?a"),
        make_rewrite("invert-involutive-right", "?a", "Inv(Inv(?a))"),
        make_rewrite("invert-mat-prod", "Inv(Mul(?a, ?b))", "Mul(Inv(?b), Inv(?a))"),
        make_rewrite("invert-mat-prod-right", "Mul(Inv(?b), Inv(?a))", "Inv(Mul(?a, ?b))"),

        make_rewrite(
            "invert-cancel-left", "Mul(Inv(?a), ?a)", "?__dynamic__", nullptr,
            [](EGraph &g, const Substitution &s) {
        return make_identity_for(g, s, "a");
    }),
        make_rewrite(
            "invert-cancel-right", "Mul(?a, Inv(?a))", "?__dynamic__", nullptr,
            [](EGraph &g, const Substitution &s) {
        return make_identity_for(g, s, "a");
    }),

        make_rewrite(
            "add-comm-zero", "Add(?a, ?z)", "?a",
            [](const EGraph &g, const Substitution &s) {
        return is_zero(s, g, "z");
    }),
        make_rewrite(
            "mul-zero-left", "Mul(?z, ?a)", "?z",
            [](const EGraph &g, const Substitution &s) {
        return is_zero(s, g, "z");
    }),
        make_rewrite(
            "mul-zero-right", "Mul(?a, ?z)", "?z",
            [](const EGraph &g, const Substitution &s) {
        return is_zero(s, g, "z");
    }),

        make_rewrite("negate-involutive", "Neg(Neg(?a))", "?a"),
        make_rewrite("negate-involutive-right", "?a", "Neg(Neg(?a))"),

        make_rewrite(
            "add-negate-cancel-left", "Add(?a, Neg(?a))", "?__dynamic__", nullptr,
            [](EGraph &g, const Substitution &s) {
        return make_zero_for(g, s, "a");
    }),

        make_rewrite("transpose-invert", "Tr(Inv(?a))", "Inv(Tr(?a))"),
        make_rewrite("invert-transpose", "Inv(Tr(?a))", "Tr(Inv(?a))"),
        make_rewrite("transpose-add", "Tr(Add(?a, ?b))", "Add(Tr(?a), Tr(?b))"),
        make_rewrite("transpose-add-right", "Add(Tr(?a), Tr(?b))", "Tr(Add(?a, ?b))"),
    };
}

static const auto invert_cancel_left = make_rewrite(
    "invert-cancel-left", "Mul(Inv(?a), ?a)", "?__dynamic__", nullptr, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});
static const auto invert_cancel_right = make_rewrite(
    "invert-cancel-right", "Mul(?a, Inv(?a))", "?__dynamic__", nullptr, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});
static const auto mul_assoc_left = make_rewrite("mul-assoc-left", "Mul(?a, Mul(?b, ?c))", "Mul(Mul(?a, ?b), ?c)");
static const auto mul_assoc_right = make_rewrite("mul-assoc-right", "Mul(Mul(?a, ?b), ?c)", "Mul(?a, Mul(?b, ?c))");
static const auto mul_identity_right =
    make_rewrite("mul-identity-right", "Mul(?i, ?a)", "?a", [](const EGraph &g, const Substitution &s) {
    return is_identity(s, g, "i");
});
static const auto mul_identity_left =
    make_rewrite("mul-identity-left", "Mul(?a, ?i)", "?a", [](const EGraph &g, const Substitution &s) {
    return is_identity(s, g, "i");
});
static const auto mul_assoc = make_rewrite("mul-assoc", "Mul(?a, Mul(?b, ?c))", "Mul(Mul(?a, ?b), ?c)");
static const auto commute_add = make_rewrite("commute-add", "Add(?a, ?b)", "Add(?b, ?a)");
static const auto mat_transpose_prod = make_rewrite("mat-transpose-prod", "Tr(Mul(?a, ?b))", "Mul(Tr(?b), Tr(?a))");
static const auto qr_invert = make_rewrite("qr-invert", "Inv(?a)", "Inv(Mul(Get(QR(?a), 0), Get(QR(?a), 1)))");
static const auto qr_invert_leaf = make_rewrite(
    "qr-invert-leaf", "Inv(?a)", "Mul(Inv(Get(QR(?a), 0)), Inv(Get(QR(?a), 1)))",
    [](const EGraph &g, const Substitution &s) {
    Id a_id = s.at("a");
    const auto &node = g.at(a_id);
    if (node.get_children().size() != 0)
        return false;
    return true;
});
static const auto qr_inner_invert =
    make_rewrite("qr-inner-invert", "Inv( Mul(Tr(?a), ?a) )", "Dynamic", nullptr, [](EGraph &g, const Substitution &s) {
    static const Expression result_expr("Mul(Inv(Get(QR(?a), 1)), Inv(Tr(Get(QR(?a), 1))))");
    static const Expression qr_expr("Mul(Get(QR(?a), 0), Get(QR(?a), 1))");

    Id result = g.add_expression(result_expr, s);
    Id qr_equiv = g.add_expression(qr_expr, s);

    g.union_classes(s.at("a"), qr_equiv);

    return result;
});
static const auto lu_invert = make_rewrite("lu-invert", "Inv(?a)", "Mul(Inv(Get(LU(?a), 1)), Inv(Get(LU(?a), 0)))");
static const auto lu_invert_leaf = make_rewrite(
    "lu-invert-leaf", "Inv(?a)", "Mul(Inv(Get(LU(?a), 1)), Inv(Get(LU(?a), 0)))",
    [](const EGraph &g, const Substitution &s) {
    Id a_id = s.at("a");
    const auto &node = g.at(a_id);
    if (node.get_children().size() != 0)
        return false;
    return true;
});
static const auto llt_invert = make_rewrite(
    "llt-invert", "Inv(?a)", "Mul(Tr(Inv(Get(LLt(?a), 0))), Inv(Get(LLt(?a), 0)))",
    [](const EGraph &g, const Substitution &s) {
    Id a_id = s.at("a");
    const auto &data = g.get_class_analysis_data(a_id);

    if (auto *prop = std::get_if<MatrixProperty>(&data.property)) {
        return prop->flags.is_positive_definite && prop->flags.is_symmetric;
    }
    return false;
});
static const auto llt_invert_leaf = make_rewrite(
    "llt-invert-leaf", "Inv(?a)", "Mul(Tr(Inv(Get(LLt(?a), 0))), Inv(Get(LLt(?a), 0)))",
    [](const EGraph &g, const Substitution &s) {
    Id a_id = s.at("a");
    const auto &node = g.at(a_id);
    if (node.get_children().size() != 0)
        return false;
    return true;
});
static const auto solve_rule =
    make_rewrite("solve-rule", "Mul(Inv(?a), ?b)", "Sol(?a, ?b)", [](const EGraph &g, const Substitution &s) {
    Id a_id = s.at("a");
    const auto &data = g.get_class_analysis_data(a_id);
    if (auto *prop = std::get_if<MatrixProperty>(&data.property)) {
        return prop->is_square() && !prop->flags.is_singular;
    }
    return false;
});

// static const auto QR_introduction = make_rewrite("qr-intro", "?a", "Mul(Get(QR(?a), 0),
// Get(QR(?a), 1))", [](const EGraph &g, const Substitution &s)
//                                                  {
//     Id a_id = s.at("a");
//     auto node = g.at(a_id);
//     if (!node.has_ancestor("Inv", g))
//         return false;
//     if (node.get_children().size() != 0)
//         return false;
//     const auto &data = g.get_class_analysis_data(a_id);
//     if (auto *prop = std::get_if<MatrixProperty>(&data.property))
//     {
//         return true;
//     }
//     return false; });
static const auto invert_mat_prod = make_rewrite(
    "invert-mat-prod", "Inv(Mul(?a, ?b))", "Mul(Inv(?b), Inv(?a))", [](const EGraph &g, const Substitution &s) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto &data_a = g.get_class_analysis_data(a_id);
    const auto &data_b = g.get_class_analysis_data(b_id);
    if (auto *prop_a = std::get_if<MatrixProperty>(&data_a.property)) {
        if (auto *prop_b = std::get_if<MatrixProperty>(&data_b.property)) {
            return prop_a->is_square() && !prop_a->flags.is_singular && prop_b->is_square() &&
                   !prop_b->flags.is_singular;
        }
    }
    return false;
});
static const auto orthogonal_transpose =
    make_rewrite("orthogonal-transpose", "Mul(Tr(?a), ?a)", "Identity", [](const EGraph &g, const Substitution &s) {
    Id a_id = s.at("a");
    const auto &data = g.get_class_analysis_data(a_id);
    if (auto *prop = std::get_if<MatrixProperty>(&data.property)) {
        return prop->flags.is_orthogonal;
    }
    return false;
}, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});

static const auto orthonormal_transpose =
    make_rewrite("orthonormal-transpose", "Mul(Tr(?a), ?a)", "Identity", [](const EGraph &g, const Substitution &s) {
    Id a_id = s.at("a");
    const auto &data = g.get_class_analysis_data(a_id);
    if (auto *prop = std::get_if<MatrixProperty>(&data.property)) {
        return prop->flags.is_orthonormal;
    }
    return false;
}, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a", false);
});
