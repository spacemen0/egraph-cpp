#pragma once

#include "condition_guards.h"
#include "e_graph.h"
#include "utils.h"
#include <string>
#include <string_view>

namespace egraph {
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
static const auto cholel_invert = make_rewrite(
    "cholel-invert", "Inv(?a)", "Tr(Inv(Get(CholeL(?a), 0))) * Inv(Get(CholeL(?a), 0))", true,
    [](const EGraph &g, const Substitution &s) {
    return is_not_factorized("a")(g, s) && is_pos_def("a")(g, s) && is_symmetric("a")(g, s);
});
static const auto cholel_leaf = make_rewrite(
    "cholel-leaf", "?a", "Get(CholeL(?a), 0) * Tr(Get(CholeL(?a), 0))", false, [](const EGraph &g, const Substitution &s) {
    return is_not_factorized("a")(g, s) && is_square("a")(g, s) && is_pos_def("a")(g, s) && is_symmetric("a")(g, s);
});
static const auto cholel_to_choleu = make_rewrite(
    "cholel_to_choleu", "Get(CholeL(?a), 0)", "Tr(Get(CholeU(?a), 0))", true, [](const EGraph &g, const Substitution &s) {
    return is_pos_def("a")(g, s) && is_symmetric("a")(g, s);
});

static const std::vector<Rewrite> expansion_set = {
    qr_invert, qr_leaf, lu_invert, lu_leaf, cholel_invert, cholel_leaf, cholel_to_choleu,
};
} // namespace egraph
