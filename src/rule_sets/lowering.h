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
    auto gemv_node = ENode{{a_id, b_id, zero}, Op::Gemv_N};
    return std::make_pair(g.add_node(gemv_node), false);
});
static const auto gemv_with_c =
    make_rewrite("gemv_with_c", "?a * ?b + ?c", "Gemv_N(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_matrix("a")(g, s) && is_vector("b")(g, s) && is_vector("c")(g, s) && is_not_op("a", Op::Tr)(g, s);
});
static const auto gemv_t_without_c =
    make_rewrite("gemv_t_without_c", "Tr(?a) * ?b", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_matrix("a")(g, s) && is_vector("b")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.second, b_prop->shape.second});
    auto gemvt_node = ENode{{a_id, b_id, zero}, Op::Gemv_T};
    return std::make_pair(g.add_node(gemvt_node), false);
});
static const auto gemv_t_with_c = make_rewrite(
    "gemv_t_with_c", "Tr(?a) * ?b + ?c", "Gemv_T(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
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
    auto gemm_node = ENode{{a_id, b_id, zero}, Op::Gemm_NN};
    return std::make_pair(g.add_node(gemm_node), false);
});
static const auto gemm_with_c =
    make_rewrite("gemm_with_c", "?a * ?b + ?c", "Gemm_NN(?a, ?b, ?c)", false, is_not_vector("b"));
static const auto syrk_without_c_left = make_rewrite(
    "syrk_without_c_left", "?a * Tr(?a)", "Dynamic", false, nullptr, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    const auto *a_prop = get_matrix_data(g, a_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.first, a_prop->shape.first});
    auto syrk_node = ENode{{a_id, zero}, Op::Syrk_N};
    return std::make_pair(g.add_node(syrk_node), false);
});
static const auto syrk_without_c_right = make_rewrite(
    "syrk_without_c_right", "Tr(?a) * ?a", "Dynamic", false, nullptr, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    const auto *a_prop = get_matrix_data(g, a_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.second, a_prop->shape.second});
    auto syrk_node = ENode{{a_id, zero}, Op::Syrk_T};
    return std::make_pair(g.add_node(syrk_node), false);
});
static const auto syrk_with_c_left = make_rewrite("syrk_with_c_left", "?a * Tr(?a) + ?c", "Syrk_N(?a, ?c)", false);
static const auto syrk_with_c_right = make_rewrite("syrk_with_c_right", "Tr(?a) * ?a + ?c", "Syrk_T(?a, ?c)", false);
static const auto trsm =
    make_rewrite("trsm", "Sol(?a, ?b)", "Trsm_LN(?a, ?b)", false, [](const EGraph &g, const Substitution &s) {
    return is_square("a")(g, s) && is_triangular("a")(g, s);
});

/// Transition Rules
/// ----------------------------------------------------------
static const auto gemm_tn = make_rewrite("gemm_tn", "Gemm_NN(Tr(?a), ?b, ?c)", "Gemm_TN(?a, ?b, ?c)", false);
static const auto gemm_nt = make_rewrite("gemm_nt", "Gemm_NN(?a, Tr(?b), ?c)", "Gemm_NT(?a, ?b, ?c)", false);
static const auto gemm_tt = make_rewrite("gemm_tt", "Gemm_NN(Tr(?a), Tr(?b), ?c)", "Gemm_TT(?a, ?b, ?c)", false);

static const auto syrk_t = make_rewrite("syrk_t", "Syrk_N(Tr(?a), ?c)", "Syrk_T(?a, ?c)", false);

static const auto trsm_lt = make_rewrite("trsm_lt", "Trsm_LN(Tr(?a), ?b)", "Trsm_LT(?a, ?b)", false, is_triangular("a"));

static const auto trsm_rn = make_rewrite(
    "trsm_rn_direct", "SolR(?a, ?b)", "Trsm_RN(?a, ?b)", false, [](const EGraph &g, const Substitution &s) {
    return is_triangular("a")(g, s) && is_not_op("a", Op::Tr)(g, s);
});
static const auto trsm_rt =
    make_rewrite("trsm_rt_direct", "SolR(Tr(?a), ?b)", "Trsm_RT(?a, ?b)", false, is_triangular("a"));

/// LAPACK
/// ----------------------------------------------------------
static const auto potrf_l = make_rewrite("potrf_l", "LLt(?a)", "Potrf_L(?a)", false);
static const auto potrf_u = make_rewrite("potrf_u", "UtU(?a)", "Potrf_U(?a)", false);
static const auto geqrf = make_rewrite("geqrf", "QR(?a)", "Geqrf(?a)", false);
static const auto trtri = make_rewrite("trtri", "Inv(?a)", "Trtri(?a)", false, is_triangular("a"));

static const std::vector<Rewrite> lowering_set = {
    gemv_without_c,
    gemv_with_c,
    gemm_without_c,
    gemm_with_c,
    gemv_t_without_c,
    gemv_t_with_c,
    syrk_without_c_left,
    syrk_without_c_right,
    syrk_with_c_left,
    syrk_with_c_right,
    trsm,
    geqrf,
    trtri,
    gemm_tn,
    gemm_nt,
    gemm_tt,
    syrk_t,
    trsm_lt,
    trsm_rn,
    trsm_rt,
    potrf_l,
    potrf_u,
};