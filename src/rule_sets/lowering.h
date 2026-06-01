#pragma once

#include "condition_guards.h"
#include "e_graph.h"
#include "utils.h"
#include <string>
#include <string_view>

/// BLAS Level 2
/// ----------------------------------------------------------
static const auto gemv_without_c =
    make_rewrite("gemv_without_c", "?a * ?b", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_matrix("a")(g, s) && is_vector("b")(g, s) && is_not_op("a", Op::Tr)(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.first, b_prop->shape.second});
    auto gemv_node = ENode{{a_id, b_id, zero}, Op::Gemv};
    return std::make_pair(g.add_node(gemv_node), false);
});
static const auto gemv_with_c =
    make_rewrite("gemv_with_c", "?a * ?b + ?c", "Gemv(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_matrix("a")(g, s) && is_vector("b")(g, s) && is_vector("c")(g, s) && is_not_op("a", Op::Tr)(g, s);
});
static const auto gemvt_without_c =
    make_rewrite("gemvt_without_c", "Tr(?a) * ?b", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_matrix("a")(g, s) && is_vector("b")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.second, b_prop->shape.second});
    auto gemvt_node = ENode{{a_id, b_id, zero}, Op::Gemvt};
    return std::make_pair(g.add_node(gemvt_node), false);
});
static const auto gemvt_with_c = make_rewrite(
    "gemvt_with_c", "Tr(?a) * ?b + ?c", "Gemvt(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_matrix("a")(g, s) && is_vector("b")(g, s) && is_vector("c")(g, s);
});

/// BLAS Level 3
/// ----------------------------------------------------------
static const auto gemm_without_c = make_rewrite(
    "gemm_without_c", "?a * ?b", "Dynamic", false, is_not_vector("b"), [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.first, b_prop->shape.second});
    auto gemm_node = ENode{{a_id, b_id, zero}, Op::Gemm};
    return std::make_pair(g.add_node(gemm_node), false);
});
static const auto gemm_with_c =
    make_rewrite("gemm_with_c", "?a * ?b + ?c", "Gemm(?a, ?b, ?c)", false, is_not_vector("b"));
static const auto syrk_without_c_left = make_rewrite(
    "syrk_without_c_left", "?a * Tr(?a)", "Dynamic", false, nullptr, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    const auto *a_prop = get_matrix_data(g, a_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.first, a_prop->shape.first});
    auto syrk_node = ENode{{a_id, zero}, Op::Syrk};
    return std::make_pair(g.add_node(syrk_node), false);
});
static const auto syrk_without_c_right = make_rewrite(
    "syrk_without_c_right", "Tr(?a) * ?a", "Dynamic", false, nullptr, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    const auto *a_prop = get_matrix_data(g, a_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.second, a_prop->shape.second});
    auto transpose_a = g.add_expression(Expression("Tr(?a)"), s);
    auto syrk_node = ENode{{transpose_a, zero}, Op::Syrk};
    return std::make_pair(g.add_node(syrk_node), false);
});
static const auto syrk_with_c_left = make_rewrite("syrk_with_c_left", "?a * Tr(?a) + ?c", "Syrk(?a, ?c)", false);
static const auto syrk_with_c_right = make_rewrite("syrk_with_c_right", "Tr(?a) * ?a + ?c", "Syrk(Tr(?a), ?c)", false);
static const auto trsm =
    make_rewrite("trsm", "Sol(?a, ?b)", "Trsm(?a, ?b)", false, [](const EGraph &g, const Substitution &s) {
    return is_square("a")(g, s) && is_triangular("a")(g, s);
});

/// LAPACK
/// ----------------------------------------------------------
static const auto potrf = make_rewrite("potrf", "LLt(?a)", "Potrf(?a)", false);
static const auto geqrf = make_rewrite("geqrf", "QR(?a)", "Geqrf(?a)", false);
static const auto trtri = make_rewrite("trtri", "Inv(?a)", "Trtri(?a)", false, is_triangular("a"));

static const std::vector<Rewrite> lowering_set = {
    gemv_without_c,
    gemv_with_c,
    gemm_without_c,
    gemm_with_c,
    gemvt_without_c,
    gemvt_with_c,
    syrk_without_c_left,
    syrk_without_c_right,
    syrk_with_c_left,
    syrk_with_c_right,
    trsm,
    potrf,
    geqrf,
    trtri,
};