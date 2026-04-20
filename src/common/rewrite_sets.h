#include "e_graph.h"
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

static auto is_not_factorized = [](const EGraph &g, const Substitution &s) {
    Id a_id = s.at("a");
    const auto &data = g.get_class_analysis_data(a_id);
    if (const auto *prop = std::get_if<MatrixProperty>(&data.property)) {
        return !(
            prop->flags.is_upper_triangular || prop->flags.is_lower_triangular || prop->flags.is_diagonal ||
            prop->flags.is_identity || prop->flags.is_orthogonal || prop->flags.is_orthonormal);
    }
    return true;
};
static auto leaf_and_not_factorized = [](const EGraph &g, const Substitution &s) {
    return is_leaf_condition(g, s) && is_not_factorized(g, s);
};

static auto leaf_and_not_factorized_and_square = [](const EGraph &g, const Substitution &s) {
    return is_leaf_condition(g, s) && is_not_factorized(g, s) && check_is_square(s, g, "a");
};

static const auto qr_invert =
    make_rewrite("qr-invert", "Inv(?a)", "Inv(Mul(Get(QR(?a), 0), Get(QR(?a), 1)))", is_not_factorized);
static const auto qr_leaf =
    make_rewrite("qr-leaf", "?a", "Mul(Get(QR(?a), 0), Get(QR(?a), 1))", [](const EGraph &g, const Substitution &s) {
    if (!leaf_and_not_factorized(g, s))
        return false;
    Id a_id = s.at("a");
    const auto &data = g.get_class_analysis_data(a_id);
    if (const auto *prop = std::get_if<MatrixProperty>(&data.property)) {
        // Avoid ambiguous symbolic-shape exceptions.
        return !prop->is_vector() && (prop->is_square() || prop->is_tall_matrix() || prop->is_wide_matrix());
    }
    return false;
});

static const auto lu_invert =
    make_rewrite("lu-invert", "Inv(?a)", "Mul(Inv(Get(LU(?a), 1)), Inv(Get(LU(?a), 0)))", is_not_factorized);

static const auto lu_leaf =
    make_rewrite("lu-leaf", "?a", "Mul(Get(LU(?a), 0), Get(LU(?a), 1))", leaf_and_not_factorized_and_square);
static const auto llt_invert = make_rewrite(
    "llt-invert", "Inv(?a)", "Mul(Tr(Inv(Get(LLt(?a), 0))), Inv(Get(LLt(?a), 0)))",
    [](const EGraph &g, const Substitution &s) {
    if (!is_not_factorized(g, s))
        return false;
    Id a_id = s.at("a");
    const auto &data = g.get_class_analysis_data(a_id);

    if (auto *prop = std::get_if<MatrixProperty>(&data.property)) {
        return prop->flags.is_positive_definite && prop->flags.is_symmetric;
    }
    return false;
});

static const auto llt_leaf = make_rewrite(
    "llt-leaf", "?a", "Mul(Tr(Get(LLt(?a), 0)), Get(LLt(?a), 0))", [](const EGraph &g, const Substitution &s) {
    if (!leaf_and_not_factorized_and_square(g, s))
        return false;
    Id a_id = s.at("a");
    const auto &data = g.get_class_analysis_data(a_id);

    if (auto *prop = std::get_if<MatrixProperty>(&data.property)) {
        return prop->flags.is_positive_definite && prop->flags.is_symmetric;
    }
    return false;
});

static const auto mul_identity_left =
    make_rewrite("mul-identity-left", "Mul(?a, ?i)", "?a", [](const EGraph &g, const Substitution &s) {
    return is_identity(s, g, "i");
});

static const auto mul_identity_right =
    make_rewrite("mul-identity-right", "Mul(?i, ?a)", "?a", [](const EGraph &g, const Substitution &s) {
    return is_identity(s, g, "i");
});

static const auto mul_assoc_left = make_rewrite("mul-assoc-left", "Mul(?a, Mul(?b, ?c))", "Mul(Mul(?a, ?b), ?c)");

static const auto mul_assoc_right = make_rewrite("mul-assoc-right", "Mul(Mul(?a, ?b), ?c)", "Mul(?a, Mul(?b, ?c))");

static const auto commute_add = make_rewrite("commute-add", "Add(?a, ?b)", "Add(?b, ?a)");

static const auto mat_transpose_prod = make_rewrite("mat-transpose-prod", "Tr(Mul(?a, ?b))", "Mul(Tr(?b), Tr(?a))");

static const auto mat_transpose_prod_right =
    make_rewrite("mat-transpose-prod-right", "Mul(Tr(?b), Tr(?a))", "Tr(Mul(?a, ?b))");

static const auto invert_cancel_left = make_rewrite(
    "invert-cancel-left", "Mul(Inv(?a), ?a)", "?__dynamic__", nullptr, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});

static const auto invert_cancel_right = make_rewrite(
    "invert-cancel-right", "Mul(?a, Inv(?a))", "?__dynamic__", nullptr, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});

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

static const auto negate_involutive = make_rewrite("negate-involutive", "Neg(Neg(?a))", "?a");

static const auto add_negate_cancel_left = make_rewrite(
    "add-negate-cancel-left", "Add(?a, Neg(?a))", "?__dynamic__", nullptr, [](EGraph &g, const Substitution &s) {
    return make_zero_for(g, s, "a");
});

static const auto add_comm_zero =
    make_rewrite("add-comm-zero", "Add(?a, ?z)", "?a", [](const EGraph &g, const Substitution &s) {
    return is_zero(s, g, "z");
});

static const auto mul_zero_left =
    make_rewrite("mul-zero-left", "Mul(?z, ?a)", "Dynamic", [](const EGraph &g, const Substitution &s) {
    return is_zero(s, g, "z");
}, [](EGraph &g, const Substitution &s) {
    const auto *z_prop = get_matrix_data(g, s.at("z"));
    const auto *a_prop = get_matrix_data(g, s.at("a"));
    return make_zero_of_shape(g, {z_prop->shape.first, a_prop->shape.second});
});

static const auto mul_zero_right =
    make_rewrite("mul-zero-right", "Mul(?a, ?z)", "Dynamic", [](const EGraph &g, const Substitution &s) {
    return is_zero(s, g, "z");
}, [](EGraph &g, const Substitution &s) {
    const auto *a_prop = get_matrix_data(g, s.at("a"));
    const auto *z_prop = get_matrix_data(g, s.at("z"));
    return make_zero_of_shape(g, {a_prop->shape.first, z_prop->shape.second});
});

static const auto solver_left = make_rewrite("solver_left", "Mul(Inv(?a), ?b)", "Sol(?a, ?b)");
static const auto solve_composition = make_rewrite(
    "solve_composition", "Sol(Mul(?a, ?b), ?c)", "Sol(?b, Sol(?a, ?c))", [](const EGraph &g, const Substitution &s) {
    return check_is_square(s, g, "a") && check_is_square(s, g, "b");
});
static const auto solve_cancel_left = make_rewrite("solve_cancel_left", "Sol(?a, Mul(?a, ?b))", "?b");
static const auto solve_cancel_right = make_rewrite("solve_cancel_right", "Mul(?a, Sol(?a, ?b))", "?b");
static const auto solve_by_id =
    make_rewrite("solve_by_id", "Sol(?b, ?a)", "?a", [](const EGraph &g, const Substitution &s) {
    return is_identity(s, g, "b");
}, nullptr);
static const auto solve_identity =
    make_rewrite("solve_identity", "Sol(?a, ?a)", "Dynamic", nullptr, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});
static const auto inverse_solve = make_rewrite("inverse_solve", "Inv(Sol(?a, ?b))", "Mul(Inv(?b), ?a)");
static const auto transpose_solve = make_rewrite("transpose_solve", "Tr(Sol(?a, ?b))", "SolR(Tr(?b), Tr(?a))");

static const auto solver_right = make_rewrite("solver_right", "Mul(?b, Inv(?a))", "SolR(?b, ?a)");
static const auto solver_right_composition = make_rewrite(
    "solver_right_composition", "SolR(?c, Mul(?a, ?b))", "SolR(SolR(?c, ?b), ?a)",
    [](const EGraph &g, const Substitution &s) {
    return check_is_square(s, g, "a") && check_is_square(s, g, "b");
});
static const auto solve_r_cancel_left = make_rewrite("solve_r_cancel_left", "SolR(Mul(?b, ?a), ?a)", "?b");
static const auto solve_r_cancel_right = make_rewrite("solve_r_cancel_right", "Mul(SolR(?b, ?a), ?a)", "?b");
static const auto solve_r_identity =
    make_rewrite("solve_r_identity", "SolR(?a, ?a)", "Dynamic", nullptr, [](EGraph &g, const Substitution &s) {
    return make_identity_for(g, s, "a");
});
static const auto solve_r_by_id =
    make_rewrite("solve_r_by_id", "SolR(?a, ?b)", "?a", [](const EGraph &g, const Substitution &s) {
    return is_identity(s, g, "b");
}, nullptr);
static const auto inverse_solve_r = make_rewrite("inverse_solve_r", "Inv(SolR(?b, ?a))", "SolR(?a, ?b)");
static const auto transpose_solve_r = make_rewrite("transpose_solve_r", "Tr(SolR(?b, ?a))", "Sol(Tr(?a), Tr(?b))");

static const std::vector<Rewrite> factorization_set = {qr_invert, lu_invert, llt_invert, qr_leaf, lu_leaf, llt_leaf};

static const std::vector<Rewrite> algebraic_set = {
    mul_identity_left, mul_identity_right, mul_assoc_left,           mul_assoc_right,
    commute_add,       mat_transpose_prod, mat_transpose_prod_right,
};

static const std::vector<Rewrite> inverse_set = {
    invert_cancel_left,
    invert_cancel_right,
    invert_mat_prod,
};

static const std::vector<Rewrite> orthogonality_set = {
    orthogonal_transpose,
    orthonormal_transpose,
};

static const std::vector<Rewrite> zero_negation_set = {
    negate_involutive, add_negate_cancel_left, add_comm_zero, mul_zero_left, mul_zero_right,
};

static const std::vector<Rewrite> solver_set = {
    solver_left,   solve_composition,        solve_cancel_left,   solve_cancel_right,
    solve_by_id,   solve_identity,           inverse_solve,       transpose_solve,
    solver_right,  solver_right_composition, solve_r_cancel_left, solve_r_cancel_right,
    solve_r_by_id, solve_r_identity,         inverse_solve_r,     transpose_solve_r,
};

static std::vector<Rewrite> build_complete_rewrite_set() {
    std::vector<Rewrite> rewrites;
    rewrites.insert(rewrites.end(), algebraic_set.begin(), algebraic_set.end());
    rewrites.insert(rewrites.end(), inverse_set.begin(), inverse_set.end());
    rewrites.insert(rewrites.end(), orthogonality_set.begin(), orthogonality_set.end());
    rewrites.insert(rewrites.end(), zero_negation_set.begin(), zero_negation_set.end());
    rewrites.insert(rewrites.end(), factorization_set.begin(), factorization_set.end());
    rewrites.insert(rewrites.end(), solver_set.begin(), solver_set.end());
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
        return algebraic_set;
    }
    if (name == "inverse") {
        return inverse_set;
    }
    if (name == "orthogonality") {
        return orthogonality_set;
    }
    if (name == "zero-negation") {
        return zero_negation_set;
    }
    if (name == "solver") {
        return solver_set;
    }
    throw std::invalid_argument("Unknown rewrite set name: " + name);
}

inline std::vector<Rewrite> build_rewrite_sets(std::initializer_list<std::string_view> set_names) {
    std::vector<Rewrite> rules;
    for (std::string_view set_name : set_names) {
        auto set_rules = get_rewrite_set_by_name(std::string(set_name));
        rules.insert(rules.end(), set_rules.begin(), set_rules.end());
    }
    return rules;
}