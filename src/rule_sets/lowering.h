#pragma once

#include "condition_guards.h"
#include "e_graph.h"
#include "utils.h"
#include <string>
#include <string_view>

/// BLAS Level 2
/// ----------------------------------------------------------

namespace egraph {

// gemv
static const auto gemv_n_with_c = make_rewrite(
    "gemv_n_with_c", "?a * ?b + ?c", "Gemv_N(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_matrix("a")(g, s) && is_vector("b")(g, s) && is_vector("c")(g, s) && is_not_op("a", Op::Tr)(g, s);
});

static const auto gemv_n_without_c =
    make_rewrite("gemv_n_without_c", "?a * ?b", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_matrix("a")(g, s) && is_vector("b")(g, s) && is_not_op("a", Op::Tr)(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.first, b_prop->shape.second});
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Gemv_N}), false);
}, 30, {Op::Gemv_N});

static const auto gemv_t_with_c = make_rewrite(
    "gemv_t_with_c", "Tr(?a) * ?b + ?c", "Gemv_T(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_matrix("a")(g, s) && is_vector("b")(g, s) && is_vector("c")(g, s);
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
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Gemv_T}), false);
}, 30, {Op::Gemv_T});

/// symm
static const auto symm_l_without_c =
    make_rewrite("symm_l_without_c", "?a * ?b", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_symmetric("a")(g, s) && is_not_vector("b")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.first, b_prop->shape.second});
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Symm_L}), false);
}, 30, {Op::Symm_L});
static const auto symm_l =
    make_rewrite("symm_l", "?a * ?b + ?c", "Symm_L(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_symmetric("a")(g, s) && is_not_vector("b")(g, s);
});

static const auto symm_r_without_c =
    make_rewrite("symm_r_without_c", "?b * ?a", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_symmetric("a")(g, s) && is_not_vector("b")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {b_prop->shape.first, a_prop->shape.second});
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Symm_R}), false);
}, 30, {Op::Symm_R});
static const auto symm_r =
    make_rewrite("symm_r", "?b * ?a + ?c", "Symm_R(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_symmetric("a")(g, s) && is_not_vector("b")(g, s);
});

/// trmm
static const auto trmm_ln_without_c =
    make_rewrite("trmm_ln_without_c", "?a * ?b", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_triangular("a")(g, s) && is_not_vector("b")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.first, b_prop->shape.second});
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Trmm_LN}), false);
}, 30, {Op::Trmm_LN});
static const auto trmm_ln =
    make_rewrite("trmm_ln", "?a * ?b + ?c", "Trmm_LN(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_triangular("a")(g, s) && is_not_vector("b")(g, s);
});

static const auto trmm_lt_without_c =
    make_rewrite("trmm_lt_without_c", "Tr(?a) * ?b", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_triangular("a")(g, s) && is_not_vector("b")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.second, b_prop->shape.second});
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Trmm_LT}), false);
}, 30, {Op::Trmm_LT});
static const auto trmm_lt = make_rewrite(
    "trmm_lt", "Tr(?a) * ?b + ?c", "Trmm_LT(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_triangular("a")(g, s) && is_not_vector("b")(g, s);
});

static const auto trmm_rn_without_c =
    make_rewrite("trmm_rn_without_c", "?b * ?a", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_triangular("a")(g, s) && is_not_vector("b")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {b_prop->shape.first, a_prop->shape.second});
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Trmm_RN}), false);
}, 30, {Op::Trmm_RN});
static const auto trmm_rn =
    make_rewrite("trmm_rn", "?b * ?a + ?c", "Trmm_RN(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_triangular("a")(g, s) && is_not_vector("b")(g, s);
});

static const auto trmm_rt_without_c =
    make_rewrite("trmm_rt_without_c", "?b * Tr(?a)", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_triangular("a")(g, s) && is_not_vector("b")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {b_prop->shape.first, a_prop->shape.first});
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Trmm_RT}), false);
}, 30, {Op::Trmm_RT});
static const auto trmm_rt = make_rewrite(
    "trmm_rt", "?b * Tr(?a) + ?c", "Trmm_RT(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_triangular("a")(g, s) && is_not_vector("b")(g, s);
});

/// BLAS Level 3: syrk
/// ----------------------------------------------------------
static const auto syrk_n_with_c =
    make_rewrite("syrk_n_with_c", "?a * Tr(?a) + ?c", "Syrk_N(?a, ?c)", false, is_symmetric("c"));

static const auto syrk_n_without_c = make_rewrite(
    "syrk_n_without_c", "?a * Tr(?a)", "Dynamic", false, nullptr, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    const auto *a_prop = get_matrix_data(g, a_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.first, a_prop->shape.first});
    return std::make_pair(g.add_node(ENode{{a_id, zero}, Op::Syrk_N}), false);
}, 30, {Op::Syrk_N});

static const auto syrk_t_with_c =
    make_rewrite("syrk_t_with_c", "Tr(?a) * ?a + ?c", "Syrk_T(?a, ?c)", false, is_symmetric("c"));

static const auto syrk_t_without_c = make_rewrite(
    "syrk_t_without_c", "Tr(?a) * ?a", "Dynamic", false, nullptr, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    const auto *a_prop = get_matrix_data(g, a_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.second, a_prop->shape.second});
    return std::make_pair(g.add_node(ENode{{a_id, zero}, Op::Syrk_T}), false);
}, 30, {Op::Syrk_T});

/// BLAS Level 3: gemm
/// ----------------------------------------------------------
static const auto gemm_tn_without_c =
    make_rewrite("gemm_tn_without_c", "Tr(?a) * ?b", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_not_vector("b")(g, s) && is_not_vector("a")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.second, b_prop->shape.second});
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Gemm_TN}), false);
}, 30, {Op::Gemm_TN});
static const auto gemm_tn = make_rewrite(
    "gemm_tn", "Tr(?a) * ?b + ?c", "Gemm_TN(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_not_vector("b")(g, s) && is_not_vector("a")(g, s);
});

static const auto gemm_nt_without_c =
    make_rewrite("gemm_nt_without_c", "?a * Tr(?b)", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_not_vector("a")(g, s) && is_not_vector("b")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.first, b_prop->shape.first});
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Gemm_NT}), false);
}, 30, {Op::Gemm_NT});
static const auto gemm_nt = make_rewrite(
    "gemm_nt", "?a * Tr(?b) + ?c", "Gemm_NT(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_not_vector("a")(g, s) && is_not_vector("b")(g, s);
});

static const auto gemm_tt_without_c =
    make_rewrite("gemm_tt_without_c", "Tr(?a) * Tr(?b)", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_not_vector("b")(g, s) && is_not_vector("a")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.second, b_prop->shape.first});
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Gemm_TT}), false);
}, 30, {Op::Gemm_TT});
static const auto gemm_tt = make_rewrite(
    "gemm_tt", "Tr(?a) * Tr(?b) + ?c", "Gemm_TT(?a, ?b, ?c)", false, [](const EGraph &g, const Substitution &s) {
    return is_not_vector("b")(g, s) && is_not_vector("a")(g, s);
});
static const auto gemm_with_c =
    make_rewrite("gemm_with_c", "?a * ?b + ?c", "Gemm_NN(?a, ?b, ?c)", false, is_not_vector("b"));

static const auto gemm_without_c =
    make_rewrite("gemm_without_c", "?a * ?b", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_not_vector("b")(g, s) && is_not_vector("a")(g, s);
}, [](EGraph &g, const Substitution &s, Id _) {
    Id a_id = s.at("a");
    Id b_id = s.at("b");
    const auto *a_prop = get_matrix_data(g, a_id);
    const auto *b_prop = get_matrix_data(g, b_id);
    auto zero = make_zero_of_shape(g, {a_prop->shape.first, b_prop->shape.second});
    return std::make_pair(g.add_node(ENode{{a_id, b_id, zero}, Op::Gemm_NN}), false);
}, 30, {Op::Gemm_NN});

/// BLAS: trsm
/// ----------------------------------------------------------
static const auto trsm_ln = make_rewrite("trsm_ln", "Inv(?a) * ?b", "Trsm_LN(?a, ?b)", false, is_triangular("a"));
static const auto trsm_lt_tr_inv =
    make_rewrite("trsm_lt_tr_inv", "Tr(Inv(?a)) * ?b", "Trsm_LT(?a, ?b)", false, is_triangular("a"));
static const auto trsm_rn = make_rewrite("trsm_rn", "?b * Inv(?a)", "Trsm_RN(?a, ?b)", false, is_triangular("a"));
static const auto trsm_rt_tr_inv =
    make_rewrite("trsm_rt_tr_inv", "?b * Tr(Inv(?a))", "Trsm_RT(?a, ?b)", false, is_triangular("a"));

/// LAPACK
/// ----------------------------------------------------------
static const auto potrf_l = make_rewrite("potrf_l", "CholeL(?a)", "Potrf_L(?a)", false);
static const auto potrf_u = make_rewrite("potrf_u", "CholeU(?a)", "Potrf_U(?a)", false);
static const auto potrf_u_to_potrf_l =
    make_rewrite("potrf_u_to_potrf_l", "Get(Potrf_U(?a), 0)", "Tr(Get(Potrf_L(?a), 0))", false);
static const auto potrf_l_to_potrf_u =
    make_rewrite("potrf_l_to_potrf_u", "Get(Potrf_L(?a), 0)", "Tr(Get(Potrf_U(?a), 0))", false);
static const auto geqrf = make_rewrite("geqrf", "QR(?a)", "Geqrf(?a)", false);
static const auto get_orgqr = make_rewrite("get_orgqr", "Get(Geqrf(?a), 0)", "Orgqr(Geqrf(?a))", false);
static const auto fuse_ormqr_ln =
    make_rewrite("fuse_ormqr_ln", "Get(Geqrf(?a), 0) * ?b", "Ormqr_LN(Geqrf(?a), ?b)", false);
static const auto fuse_ormqr_lt =
    make_rewrite("fuse_ormqr_lt", "Tr(Get(Geqrf(?a), 0)) * ?b", "Ormqr_LT(Geqrf(?a), ?b)", false);
static const auto fuse_ormqr_rn =
    make_rewrite("fuse_ormqr_rn", "?b * Get(Geqrf(?a), 0)", "Ormqr_RN(Geqrf(?a), ?b)", false);
static const auto fuse_ormqr_rt =
    make_rewrite("fuse_ormqr_rt", "?b * Tr(Get(Geqrf(?a), 0))", "Ormqr_RT(Geqrf(?a), ?b)", false);

static const auto trtri = make_rewrite("trtri", "Inv(?a)", "Trtri(?a)", false, is_triangular("a"));

/// Vector ops
/// ----------------------------------------------------------
static const auto axpy = make_rewrite("axpy", "?a + ?b", "Axpy(?a, ?b)", false);
static const auto axpy_minus = make_rewrite("axpy_minus", "?a - ?b", "Axpy(?a, Scale(?b, -1))", false);

static const std::vector<Rewrite> lowering_set = {
    gemv_n_with_c,
    gemv_n_without_c,
    gemv_t_with_c,
    gemv_t_without_c,
    symm_l_without_c,
    symm_l,
    symm_r_without_c,
    symm_r,
    trmm_ln_without_c,
    trmm_ln,
    trmm_lt_without_c,
    trmm_lt,
    trmm_rn_without_c,
    trmm_rn,
    trmm_rt_without_c,
    trmm_rt,
    syrk_n_with_c,
    syrk_n_without_c,
    syrk_t_with_c,
    syrk_t_without_c,
    trsm_ln,
    trsm_lt_tr_inv,
    trsm_rn,
    trsm_rt_tr_inv,
    gemm_tn_without_c,
    gemm_tn,
    gemm_nt_without_c,
    gemm_nt,
    gemm_tt_without_c,
    gemm_tt,
    gemm_with_c,
    gemm_without_c,
    potrf_l,
    potrf_u,
    potrf_u_to_potrf_l,
    potrf_l_to_potrf_u,
    geqrf,
    get_orgqr,
    fuse_ormqr_ln,
    fuse_ormqr_lt,
    fuse_ormqr_rn,
    fuse_ormqr_rt,
    trtri,
    axpy,
    axpy_minus,
};
} // namespace egraph
