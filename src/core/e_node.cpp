#include "e_node.h"
#include "op_costs.h"
#include "basic_types.h"
#include "e_graph.h"
#include "types.h"
#include "utils.h"
#include <algorithm>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <string>

const Children &ENode::get_children() const { return children; }
Children &ENode::get_children_mut() { return children; }

Atom ENode::get_atom() const { return atom; }

std::string ENode::to_string() const { return atom_to_string(atom); }

std::string ENode::format() const {
    if (is_leaf()) {
        return to_string();
    }

    std::string str = "(" + to_string();
    std::ranges::for_each(children, [&](Id child_id) {
        str += " " + std::to_string(child_id);
    });
    str += ")";
    return str;
}

size_t ENode::hash() const {
    size_t seed;
    if (std::holds_alternative<Op>(atom)) {
        Op op = std::get<Op>(atom);
        seed = std::hash<int>()(static_cast<std::underlying_type_t<Op>>(op));
    } else if (std::holds_alternative<uint32_t>(atom)) {
        // use a fixed discriminant for string payloads;
        seed = std::hash<int>()(-1);
    } else {
        // double payload
        seed = std::hash<int>()(-2);
    }

    seed = std::accumulate(children.begin(), children.end(), seed, [](size_t acc, Id c) {
        size_t hc = std::hash<Id>()(c);
        return acc ^ (hc + 0x9e3779b97f4a7c15ULL + (acc << 6) + (acc >> 2));
    });

    // if string payloads, mix in the string hash
    if (std::holds_alternative<uint32_t>(atom)) {
        const auto &s = std::get<uint32_t>(atom);
        size_t hp = std::hash<uint32_t>()(s);
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    } else if (std::holds_alternative<double>(atom)) {
        double d = std::get<double>(atom);
        size_t hp = std::hash<double>()(d);
        seed ^= hp + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    return seed;
}

bool ENode::is_leaf() const { return children.empty(); }

Cost ENode::compute_local_cost(const EGraph &egraph, const SizeBindings *size_bindings) const {
    if (is_leaf()) {
        return 0.0;
    }
    
    
    if (auto op = std::get_if<Op>(&atom)) {
        switch (*op) {
            using enum Op;
        case Add:
            return compute_add_cost(*op, *this, egraph, size_bindings);
        case Mul:
            return compute_mul_cost(*op, *this, egraph, size_bindings);
        case Tr:
            return compute_tr_cost(*op, *this, egraph, size_bindings);
        case Inv:
            return compute_inv_cost(*op, *this, egraph, size_bindings);
        case Minus:
            return compute_minus_cost(*op, *this, egraph, size_bindings);
        case QR:
            return compute_qr_cost(*op, *this, egraph, size_bindings);
        case LU:
            return compute_lu_cost(*op, *this, egraph, size_bindings);
        case LLt:
            return compute_llt_cost(*op, *this, egraph, size_bindings);
        case UtU:
            return compute_utu_cost(*op, *this, egraph, size_bindings);
        case Get:
            return compute_get_cost(*op, *this, egraph, size_bindings);
        case Sol:
            return compute_sol_cost(*op, *this, egraph, size_bindings);
        case SolR:
            return compute_solr_cost(*op, *this, egraph, size_bindings);
        case Scale:
            return compute_scale_cost(*op, *this, egraph, size_bindings);
        case Det:
            return compute_det_cost(*op, *this, egraph, size_bindings);
        case Log:
            return compute_log_cost(*op, *this, egraph, size_bindings);
        case Gemm_NN:
        case Gemm_TN:
        case Gemm_NT:
        case Gemm_TT:
            return compute_gemm_nn_group_cost(*op, *this, egraph, size_bindings);
        case Syrk_N:
        case Syrk_T:
            return compute_syrk_n_syrk_t_cost(*op, *this, egraph, size_bindings);
        case Trsm_LN:
        case Trsm_LT:
        case Trsm_RN:
        case Trsm_RT:
            return compute_trsm_ln_group_cost(*op, *this, egraph, size_bindings);
        case Potrf_L:
        case Potrf_U:
            return compute_potrf_l_potrf_u_cost(*op, *this, egraph, size_bindings);
        case Geqrf:
            return compute_geqrf_cost(*op, *this, egraph, size_bindings);
        case Trtri:
            return compute_trtri_cost(*op, *this, egraph, size_bindings);
        case Gemv_N:
            return compute_gemv_n_cost(*op, *this, egraph, size_bindings);
        case Gemv_T:
            return compute_gemv_t_cost(*op, *this, egraph, size_bindings);

        case Orgqr:
            return compute_orgqr_cost(*op, *this, egraph, size_bindings);
        case Ormqr_LN:
        case Ormqr_LT:
        case Ormqr_RN:
        case Ormqr_RT:
            return compute_ormqr_cost(*op, *this, egraph, size_bindings);
        default:
            throw std::invalid_argument("Unknown Op in compute_local_cost");
        }
    }
    throw std::invalid_argument("ENode with non-Op atom should not have children");
}
