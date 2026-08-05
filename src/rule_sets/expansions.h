#pragma once

#include "condition_guards.h"
#include "e_graph.h"
#include "utils.h"
#include <string>
#include <string_view>

/// Solver Lowering
/// ----------------------------------------------------------
static const auto solver_left = make_rewrite("solver_left", "Inv(?a) * ?b", "Sol(?a, ?b)");
static const auto solver_right = make_rewrite("solver_right", "?b * Inv(?a)", "SolR(?a, ?b)");
static const auto solver_right_to_left =
    make_rewrite("solver_right_to_left", "SolR(?a, ?b)", "Tr(Sol(Tr(?a), Tr(?b)))", true);
static const auto solver_left_to_right =
    make_rewrite("solver_left_to_right", "Tr(Sol(?a, ?b))", "SolR(Tr(?a), Tr(?b))", true);
/// QR Factorization
/// ----------------------------------------------------------
static const auto qr_invert =
    make_rewrite("qr-invert", "Inv(?a)", "Inv(Get(QR(?a), 0) * Get(QR(?a), 1))", true, is_not_factorized("a"));
static const auto qr_leaf =
    make_rewrite("qr-leaf", "?a", "Get(QR(?a), 0) * Get(QR(?a), 1)", true, [](const EGraph &g, const Substitution &s) {
    if (!leaf_and_not_factorized("a")(g, s))
        return false;
    const auto *prop = get_matrix_data(g, s.at("a"));
    if (prop) {
        // Avoid ambiguous symbolic-shape exceptions.
        return !prop->is_vector() && (prop->is_square() || prop->is_tall_matrix() || prop->is_wide_matrix());
    }
    return false;
});

/// LU Factorization
/// ----------------------------------------------------------
static const auto lu_invert = make_rewrite(
    "lu-invert", "Inv(?a)", "Inv(Get(LU(?a), 1)) * Inv(Get(LU(?a), 0))", true,
    [](const EGraph &g, const Substitution &s) {
    return is_not_factorized("a")(g, s) && is_square("a")(g, s);
});
static const auto lu_leaf =
    make_rewrite("lu-leaf", "?a", "Get(LU(?a), 0) * Get(LU(?a), 1)", true, leaf_and_not_factorized_and_square("a"));

/// Cholesky Factorization
/// ----------------------------------------------------------
static const auto llt_invert = make_rewrite(
    "llt-invert", "Inv(?a)", "Tr(Inv(Get(LLt(?a), 0))) * Inv(Get(LLt(?a), 0))", true,
    [](const EGraph &g, const Substitution &s) {
    return is_not_factorized("a")(g, s) && is_pos_def("a")(g, s) && is_symmetric("a")(g, s);
});
static const auto llt_leaf = make_rewrite(
    "llt-leaf", "?a", "Get(LLt(?a), 0) * Tr(Get(LLt(?a), 0))", false, [](const EGraph &g, const Substitution &s) {
    return is_not_factorized("a")(g, s) && is_square("a")(g, s) && is_pos_def("a")(g, s) && is_symmetric("a")(g, s);
});
static const auto llt_to_utu = make_rewrite(
    "llt_to_utu", "Get(LLt(?a), 0)", "Tr(Get(UtU(?a), 0))", true, [](const EGraph &g, const Substitution &s) {
    return is_pos_def("a")(g, s) && is_symmetric("a")(g, s);
});

static const std::vector<Rewrite> expansion_set = {
    solver_left, solver_right, solver_right_to_left, solver_left_to_right, qr_invert, qr_leaf, lu_invert, lu_leaf,
    llt_invert,  llt_leaf,     llt_to_utu,
};