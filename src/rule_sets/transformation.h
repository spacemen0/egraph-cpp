#pragma once

#include "condition_guards.h"
#include "e_graph.h"
#include "utils.h"
#include <string>
#include <string_view>

/// Commutativity and Associativity
/// ----------------------------------------------------------
static const auto mul_assoc = make_rewrite("mul-assoc-left", "?a * (?b * ?c)", "(?a * ?b) * ?c", true);
static const auto commute_add = make_rewrite("commute-add", "?a + ?b", "?b + ?a");

/// Distributions and Normalizations
/// ----------------------------------------------------------
static const auto mul_distribute_left =
    make_rewrite("mul-distribute-over-add-left", "?a * (?b + ?c)", "?a * ?b + ?a * ?c", true);
static const auto mul_distribute_right =
    make_rewrite("mul-distribute-over-add-right", "(?a + ?b) * ?c", "?a * ?c + ?b * ?c", true);
static const auto sub_to_add_scale = make_rewrite("sub_to_add_scale", "?a - ?b", "?a + Scale(?b, -1)");
static const auto scale_add_distribute =
    make_rewrite("scale_add_distribute", "Scale(?a + ?b, ?s)", "Scale(?a, ?s) + Scale(?b, ?s)", true);
static const auto scale_mul_distribute_left =
    make_rewrite("scale_mul_distribute_left", "Scale(?a * ?b, ?s)", "Scale(?a, ?s) * ?b", true);
static const auto scale_mul_distribute_right =
    make_rewrite("scale_mul_distribute_right", "Scale(?a * ?b, ?s)", "?a * Scale(?b, ?s)", true);

/// Transpositions and Inversions
/// ----------------------------------------------------------
static const auto invert_mat_prod = make_rewrite(
    "invert-mat-prod", "Inv(?a * ?b)", "Inv(?b) * Inv(?a)", true, [](const EGraph &g, const Substitution &s) {
    return is_non_singular_cond("a")(g, s) && is_non_singular_cond("b")(g, s);
});
static const auto mat_transpose_prod = make_rewrite("mat-transpose-prod", "Tr(?a * ?b)", "Tr(?b) * Tr(?a)", true);
static const auto scale_transpose = make_rewrite("scale_transpose", "Tr(Scale(?a, ?s))", "Scale(Tr(?a), ?s)", true);

// fix when I have double scalars
// static const auto scale_inverse = make_rewrite("scale_inverse", "Inv(Scale(?a, ?s))", "Scale(Inv(?a), 1 / ?s)",
// true);

/// ----------------------------------------------------------
static const auto solve_composition = make_rewrite(
    "solve_composition", "Sol(?a * ?b, ?c)", "Sol(?b, Sol(?a, ?c))", true, [](const EGraph &g, const Substitution &s) {
    return is_square("a")(g, s) && is_square("b")(g, s);
});
static const auto inverse_solve =
    make_rewrite("inverse_solve", "Inv(Sol(?a, ?b))", "Sol(?b, ?a)", false, [](const EGraph &g, const Substitution &s) {
    return is_square("a")(g, s) && is_square("b")(g, s);
});

static const std::vector<Rewrite> transformation_set = {
    mul_assoc,        commute_add,          mul_distribute_left,       mul_distribute_right,
    sub_to_add_scale, scale_add_distribute, scale_mul_distribute_left, scale_mul_distribute_right,
    invert_mat_prod,  mat_transpose_prod,   scale_transpose,           solve_composition,
    inverse_solve,
};