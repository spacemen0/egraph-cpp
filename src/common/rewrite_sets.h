#include "rewriter.h"
#include "utils.h"
#include <vector>

static auto is_leaf_condition = [](const EGraph &g, const Substitution &s) {
    Id a_id = s.at("a");
    const auto &node = g.at(a_id);
    if (node.get_children().size() != 0)
        return false;
    return true;
};

static const Rewrite qr_invert_leaf =
    make_rewrite("qr-invert-leaf", "Inv(?a)", "Mul(Inv(Get(QR(?a), 0)), Inv(Get(QR(?a), 1)))", is_leaf_condition);

static const Rewrite qr_inner_invert =
    make_rewrite("qr-inner-invert", "Inv( Mul(Tr(?a), ?a) )", "Dynamic", nullptr, [](EGraph &g, const Substitution &s) {
    static const Expression result_expr("Mul(Inv(Get(QR(?a), 1)), Inv(Tr(Get(QR(?a), 1))))");
    static const Expression qr_expr("Mul(Get(QR(?a), 0), Get(QR(?a), 1))");

    Id result = g.add_expression(result_expr, s);
    Id qr_equiv = g.add_expression(qr_expr, s);

    g.union_classes(s.at("a"), qr_equiv);

    return result;
});

static const Rewrite lu_invert_leaf =
    make_rewrite("lu-invert-leaf", "Inv(?a)", "Mul(Inv(Get(LU(?a), 1)), Inv(Get(LU(?a), 0)))", is_leaf_condition);

static const Rewrite llt_invert_leaf = make_rewrite(
    "llt-invert-leaf", "Inv(?a)", "Mul(Tr(Inv(Get(LLt(?a), 0))), Inv(Get(LLt(?a), 0)))",
    [](const EGraph &g, const Substitution &s) {
    Id a_id = s.at("a");
    const auto &node = g.at(a_id);
    if (node.get_children().size() != 0)
        return false;
    const auto &data = g.get_class_analysis_data(a_id);

    if (auto *prop = std::get_if<MatrixProperty>(&data.property)) {
        return prop->flags.is_positive_definite && prop->flags.is_symmetric;
    }
    return false;
});

static const Rewrite mul_identity_left =
    make_rewrite("mul-identity-left", "Mul(?a, ?i)", "?a", [](const EGraph &g, const Substitution &s) {
    return is_identity(s, g, "i");
});

static const Rewrite mul_identity_right =
    make_rewrite("mul-identity-right", "Mul(?i, ?a)", "?a", [](const EGraph &g, const Substitution &s) {
    return is_identity(s, g, "i");
});

static const Rewrite mul_assoc_left = make_rewrite("mul-assoc-left", "Mul(?a, Mul(?b, ?c))", "Mul(Mul(?a, ?b), ?c)");

static const Rewrite mul_assoc_right = make_rewrite("mul-assoc-right", "Mul(Mul(?a, ?b), ?c)", "Mul(?a, Mul(?b, ?c))");

static const Rewrite commute_add = make_rewrite("commute-add", "Add(?a, ?b)", "Add(?b, ?a)");

static const Rewrite mat_transpose_prod = make_rewrite("mat-transpose-prod", "Tr(Mul(?a, ?b))", "Mul(Tr(?b), Tr(?a))");

static const Rewrite mat_transpose_prod_right =
    make_rewrite("mat-transpose-prod-right", "Mul(Tr(?b), Tr(?a))", "Tr(Mul(?a, ?b))");

static const Rewrite invert_cancel_left = make_rewrite(
    "invert-cancel-left", "Mul(Inv(?a), ?a)", "?__dynamic__", nullptr, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});

static const Rewrite invert_cancel_right = make_rewrite(
    "invert-cancel-right", "Mul(?a, Inv(?a))", "?__dynamic__", nullptr, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});

static const Rewrite invert_mat_prod = make_rewrite(
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

static const Rewrite orthogonal_transpose =
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

static const Rewrite orthonormal_transpose =
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

static const Rewrite negate_involutive = make_rewrite("negate-involutive", "Neg(Neg(?a))", "?a");

static const Rewrite negate_involutive_right = make_rewrite("negate-involutive-right", "?a", "Neg(Neg(?a))");

static const Rewrite add_negate_cancel_left = make_rewrite(
    "add-negate-cancel-left", "Add(?a, Neg(?a))", "?__dynamic__", nullptr, [](EGraph &g, const Substitution &s) {
    return make_zero_for(g, s, "a");
});

static const Rewrite add_comm_zero =
    make_rewrite("add-comm-zero", "Add(?a, ?z)", "?a", [](const EGraph &g, const Substitution &s) {
    return is_zero(s, g, "z");
});

static const Rewrite mul_zero_left =
    make_rewrite("mul-zero-left", "Mul(?z, ?a)", "?z", [](const EGraph &g, const Substitution &s) {
    return is_zero(s, g, "z");
});

static const Rewrite mul_zero_right =
    make_rewrite("mul-zero-right", "Mul(?a, ?z)", "?z", [](const EGraph &g, const Substitution &s) {
    return is_zero(s, g, "z");
});

static const std::vector<Rewrite> factorization_set = {
    qr_invert_leaf,
    qr_inner_invert,
    lu_invert_leaf,
    llt_invert_leaf,
};

static const std::vector<Rewrite> basic_algebraic_rewrite_set = {
    mul_identity_left, mul_identity_right, mul_assoc_left,           mul_assoc_right,
    commute_add,       mat_transpose_prod, mat_transpose_prod_right,
};

static const std::vector<Rewrite> basic_inverse_rewrite_set = {
    invert_cancel_left,
    invert_cancel_right,
    invert_mat_prod,
};

static const std::vector<Rewrite> basic_orthogonality_rewrite_set = {
    orthogonal_transpose,
    orthonormal_transpose,
};

static const std::vector<Rewrite> basic_zero_negation_rewrite_set = {
    negate_involutive, negate_involutive_right, add_negate_cancel_left, add_comm_zero, mul_zero_left, mul_zero_right,
};

static std::vector<Rewrite> build_complete_rewrite_set() {
    std::vector<Rewrite> rewrites;
    rewrites.insert(rewrites.end(), basic_algebraic_rewrite_set.begin(), basic_algebraic_rewrite_set.end());
    rewrites.insert(rewrites.end(), basic_inverse_rewrite_set.begin(), basic_inverse_rewrite_set.end());
    rewrites.insert(rewrites.end(), basic_orthogonality_rewrite_set.begin(), basic_orthogonality_rewrite_set.end());
    rewrites.insert(rewrites.end(), basic_zero_negation_rewrite_set.begin(), basic_zero_negation_rewrite_set.end());
    return rewrites;
}

inline std::vector<Rewrite> get_rewrite_set_by_name(const std::string &name) {
    if (name == "complete") {
        return build_complete_rewrite_set();
    }
    if (name == "factorization") {
        return factorization_set;
    }
    if (name == "algebraic") {
        return basic_algebraic_rewrite_set;
    }
    if (name == "inverse") {
        return basic_inverse_rewrite_set;
    }
    if (name == "orthogonality") {
        return basic_orthogonality_rewrite_set;
    }
    if (name == "zero-negation") {
        return basic_zero_negation_rewrite_set;
    }
    throw std::invalid_argument("Unknown rewrite set name: " + name);
}
