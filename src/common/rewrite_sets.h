#include "algebraic_transformation.h"
#include "algorithmic_expansions.h"
#include "kernel_lowering.h"
#include "rewriter.h"
#include "simplification.h"
#include <string>
#include <string_view>
#include <vector>

static const std::vector<Rewrite> kernel_set = {
    gemm_without_c,    gemm_with_c, syrk_without_c_left, syrk_without_c_right, syrk_with_c_left,
    syrk_with_c_right, trsm,        gemv_without_c,      gemv_with_c,          trtri};
static const std::vector<Rewrite> factorization_set = {qr_invert, lu_invert, llt_invert, qr_leaf,
                                                       lu_leaf,   llt_leaf,  potrf,      geqrf};

static const std::vector<Rewrite> algebraic_set = {
    mul_identity_left,   mul_identity_right,   mul_assoc,          commute_add,         mat_transpose_prod,
    mul_distribute_left, mul_distribute_right, invert_cancel_left, invert_cancel_right, invert_mat_prod,
    sub_to_add_scale,
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
    minus_cancel,
    add_comm_zero,
    mul_zero_left,
    mul_zero_right,
};

static const std::vector<Rewrite> solver_set = {
    solver_left, solve_composition, solve_cancel_left, solve_cancel_right,
    solve_by_id, solve_identity,    inverse_solve,     solver_right,
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
    if (name == "zero_negation") {
        return zero_negation_set;
    }
    if (name == "solver") {
        return solver_set;
    }
    if (name == "kernel") {
        return kernel_set;
    }
    throw std::invalid_argument("Unknown rewrite set name: " + name);
}

/// Available sets: "complete", "factorization", "algebraic", "inverse", "orthogonality", "zero_negation", "solver"
inline std::vector<Rewrite> build_rewrite_sets(std::initializer_list<std::string_view> set_names) {
    std::vector<Rewrite> rules;
    for (std::string_view set_name : set_names) {
        auto set_rules = get_rewrite_set_by_name(std::string(set_name));
        rules.insert(rules.end(), set_rules.begin(), set_rules.end());
    }
    return rules;
}
