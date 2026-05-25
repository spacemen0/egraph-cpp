#pragma once

#include "basic_types.h"
#include "errors.h"
#include "parser.h"
#include "rewriter.h"
#include "types.h"
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

inline Op parse_op(std::string_view s) {
    using enum Op;
    if (s == "Add" || s == "+")
        return Add;
    if (s == "Mul" || s == "*")
        return Mul;
    if (s == "Tr")
        return Tr;
    if (s == "Inv")
        return Inv;
    if (s == "Minus" || s == "-")
        return Minus;
    if (s == "QR")
        return QR;
    if (s == "LU")
        return LU;
    if (s == "LLt")
        return LLt;
    if (s == "Get")
        return Get;
    if (s == "Sol")
        return Sol;
    if (s == "Det")
        return Det;
    if (s == "Log")
        return Log;
    if (s == "Scale")
        return Scale;
    if (s == "Geqrf")
        return Geqrf;
    if (s == "Gemm")
        return Gemm;
    if (s == "Syrk")
        return Syrk;
    if (s == "Trsm")
        return Trsm;
    if (s == "Potrf")
        return Potrf;
    if (s == "Trtri")
        return Trtri;
    if (s == "Gemv")
        return Gemv;
    throw InvalidOperationError("Unknown operation: " + std::string(s));
}

constexpr std::string_view trim(std::string_view sv) {
    constexpr std::string_view WHITESPACE = " \n\r\t\f\v";
    auto start = sv.find_first_not_of(WHITESPACE);

    if (start == std::string_view::npos) {
        return {};
    }

    sv.remove_prefix(start);

    auto end = sv.find_last_not_of(WHITESPACE);

    sv.remove_suffix(sv.size() - (end + 1));

    return sv;
}

inline ParsedAtom string_to_parsed_atom(std::string_view s) { return parser::parse_expression(s); }

inline std::string atom_to_string(const Atom &atom) {
    if (std::holds_alternative<Op>(atom)) {
        switch (std::get<Op>(atom)) {
            using enum Op;
        case Add:
            return "+";
        case Mul:
            return "*";
        case Tr:
            return "Tr";
        case Inv:
            return "Inv";
        case Minus:
            return "-";
        case QR:
            return "QR";
        case LU:
            return "LU";
        case LLt:
            return "LLt";
        case Get:
            return "Get";
        case Sol:
            return "Sol";
        case Det:
            return "Det";
        case Scale:
            return "Scale";
        case Log:
            return "Log";
        case Gemm:
            return "Gemm";
        case Syrk:
            return "Syrk";
        case Trsm:
            return "Trsm";
        case Potrf:
            return "Potrf";
        case Geqrf:
            return "Geqrf";
        case Trtri:
            return "Trtri";
        case Gemv:
            return "Gemv";
        }
        return "UnknownOp";
    } else if (std::holds_alternative<std::string>(atom)) {
        return std::get<std::string>(atom);
    } else if (std::holds_alternative<int>(atom)) {
        return std::to_string(std::get<int>(atom));
    }
    return "UnknownAtom";
}

inline Rewrite make_rewrite(
    const std::string &name, std::string_view lhs, std::string_view rhs, bool bidirectional = false,
    const std::function<bool(const EGraph &, const Substitution &)> &condition = nullptr,
    const std::function<Id(EGraph &, const Substitution &)> &applier = nullptr, size_t initial_match_limit = 30) {
    return Rewrite{name, Pattern(lhs), Pattern(rhs), bidirectional, condition, applier, initial_match_limit};
}

inline Id
make_identity_for(EGraph &egraph, const Substitution &s, const std::string &var_name, bool use_first_dim = true) {
    Id id = s.at(var_name);
    const auto &data = egraph.get_class_analysis_data(id);
    const auto *matrix_prop_ptr = std::get_if<MatrixProperty>(&data.property);
    if (!matrix_prop_ptr) {
        throw std::runtime_error("make_identity_for: Expected MatrixProperty but got TupleProperty");
    }
    const auto &matrix_prop = *matrix_prop_ptr;
    auto shape = matrix_prop.shape;

    MatrixProperty prop;
    prop.shape = use_first_dim ? std::make_pair(shape.first, shape.first) : std::make_pair(shape.second, shape.second);
    prop.flags = {
        .is_diagonal = true,
        .is_upper_triangular = true,
        .is_lower_triangular = true,
        .is_symmetric = true,
        .is_zero = false,
        .is_identity = true,
        .is_non_singular = true,
        .has_orthonormal_columns = true,
    };

    if (egraph.find_class_with_property(prop).has_value()) {
        return egraph.find_class_with_property(prop).value();
    }

    std::string size_str;
    if (use_first_dim) {
        if (auto val = std::get_if<int>(&shape.first))
            size_str = std::to_string(*val);
        else
            size_str = std::get<std::string>(shape.first);
    } else {
        if (auto val = std::get_if<int>(&shape.second))
            size_str = std::to_string(*val);
        else
            size_str = std::get<std::string>(shape.second);
    }

    std::string identity_name = "I_" + size_str + "x" + size_str;
    egraph.register_or_update_property(identity_name, prop);
    return egraph.add_node(ENode({}, identity_name));
}

inline Id make_zero_of_shape(EGraph &g, const Shape &shape) {
    MatrixProperty prop;
    prop.shape = shape;
    bool is_sq = shape.first == shape.second;
    prop.flags = {
        .is_diagonal = is_sq,
        .is_upper_triangular = is_sq,
        .is_lower_triangular = is_sq,
        .is_symmetric = is_sq,
        .is_zero = true,
        .is_identity = false,
        .is_non_singular = false,
    };

    if (g.find_class_with_property(prop).has_value()) {
        return g.find_class_with_property(prop).value();
    }

    std::string h_str, w_str;
    if (auto val = std::get_if<int>(&shape.first))
        h_str = std::to_string(*val);
    else
        h_str = std::get<std::string>(shape.first);

    if (auto val = std::get_if<int>(&shape.second))
        w_str = std::to_string(*val);
    else
        w_str = std::get<std::string>(shape.second);

    std::string zero_name = "Zero_" + h_str + "x" + w_str;

    g.register_or_update_property(zero_name, prop);

    return g.add_node(ENode({}, zero_name));
}

inline Id make_zero_for(EGraph &g, const Substitution &s, const std::string &var_name) {
    Id id = s.at(var_name);
    const auto &data = g.get_class_analysis_data(id);
    const auto *matrix_prop_ptr = std::get_if<MatrixProperty>(&data.property);
    if (!matrix_prop_ptr) {
        throw std::runtime_error("make_zero_for: Expected MatrixProperty but got TupleProperty");
    }
    return make_zero_of_shape(g, matrix_prop_ptr->shape);
}

inline std::optional<Expression>
get_representative_expression_impl(const EGraph &g, Id class_id, std::unordered_set<Id> &visiting) {
    Id root = g.find_class_id(class_id);
    if (!visiting.insert(root).second)
        return std::nullopt;

    auto cleanup = [&]() {
        visiting.erase(root);
    };

    const auto &nodes = g.get_class_nodes(root);
    if (nodes.empty())
        throw std::runtime_error("No nodes in class");

    std::vector<const ENode *> candidates;
    std::ranges::copy(nodes, std::back_inserter(candidates));
    std::ranges::sort(candidates, {}, [](const ENode *n) {
        return n->get_children().size();
    });

    // Try simpler nodes first (with fewer children)
    for (const ENode *candidate : candidates) {
        const auto &children = candidate->get_children();
        std::vector<Expression> children_exprs;
        children_exprs.reserve(children.size());

        bool success = std::ranges::all_of(children, [&](Id child_id) {
            auto res = get_representative_expression_impl(g, child_id, visiting);
            if (res)
                children_exprs.push_back(std::move(*res));
            return res.has_value();
        });

        if (success) {
            cleanup();
            return Expression(candidate->get_atom(), children_exprs);
        }
    }

    cleanup();
    return std::nullopt;
}

inline Expression get_representative_expression(const EGraph &g, Id class_id) {
    std::unordered_set<Id> visiting;
    auto result = get_representative_expression_impl(g, class_id, visiting);

    if (!result.has_value()) {
        throw std::runtime_error("Failed to extract: Target class is entirely cyclic.");
    }
    return result.value();
}

inline bool is_identity(const Substitution &s, const EGraph &g, const std::string &var) {
    Id id = s.at(var);
    if (auto prop = std::get_if<MatrixProperty>(&g.get_class_analysis_data(id).property))
        return prop->flags.is_identity;
    return false;
}

inline bool is_zero(const Substitution &s, const EGraph &g, const std::string &var) {
    Id id = s.at(var);
    if (auto prop = std::get_if<MatrixProperty>(&g.get_class_analysis_data(id).property))
        return prop->flags.is_zero;
    return false;
}

inline bool check_is_square(const Substitution &s, const EGraph &g, const std::string &var) {
    Id id = s.at(var);
    if (auto prop = std::get_if<MatrixProperty>(&g.get_class_analysis_data(id).property))
        return prop->is_square();
    return false;
}

inline const MatrixProperty *get_matrix_data(const EGraph &egraph, Id id) {
    return std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(id).property);
}

inline bool is_numeric(const Shape &shape) {
    return std::holds_alternative<int>(shape.first) && std::holds_alternative<int>(shape.second);
}

inline SizeBindings sample_size_bindings(int lower_bound, int upper_bound, std::span<const std::string_view> keys) {
    if (lower_bound > upper_bound) {
        throw std::invalid_argument("sample_size_bindings: lower_bound cannot be greater than upper_bound");
    }

    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(lower_bound, upper_bound);

    SizeBindings bindings;
    bindings.reserve(keys.size());
    for (std::string_view key : keys) {
        bindings[std::string(key)] = dist(gen);
    }
    return bindings;
}

inline SizeBindings sample_size_bindings(int lower_bound, int upper_bound, std::vector<std::string> keys) {
    if (lower_bound > upper_bound) {
        throw std::invalid_argument("sample_size_bindings: lower_bound cannot be greater than upper_bound");
    }

    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(lower_bound, upper_bound);

    SizeBindings bindings;
    bindings.reserve(keys.size());
    for (const std::string &key : keys) {
        bindings[key] = dist(gen);
    }
    return bindings;
}

inline std::vector<SizeBindings>
sample_size_bindings(size_t k, int lower_bound, int upper_bound, const std::vector<std::string> &keys) {
    std::vector<SizeBindings> samples;
    samples.reserve(k);
    for (size_t i = 0; i < k; ++i) {
        samples.push_back(sample_size_bindings(lower_bound, upper_bound, keys));
    }
    return samples;
}
