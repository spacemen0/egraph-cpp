#pragma once

#include "basic_types.h"
#include "errors.h"
#include "parser.h"
#include "rewriter.h"
#include "types.h"
#include <magic_enum.hpp>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>


namespace egraph {
inline LookupTable &get_lookup_table() {
    static LookupTable table;
    return table;
}

inline std::vector<std::string> &get_reverse_lookup() {
    static std::vector<std::string> reverse;
    return reverse;
}

inline std::string get_string_from_lookup(uint32_t id) {
    auto &reverse = get_reverse_lookup();
    if (id < reverse.size()) {
        return reverse[id];
    }
    return "<unknown>";
}

inline uint32_t register_string_in_lookup(const std::string &s) {
    auto &table = get_lookup_table();
    if (table.find(s) == table.end()) {
        uint32_t id = static_cast<uint32_t>(table.size());
        table[s] = id;
        get_reverse_lookup().push_back(s);
    }
    return table[s];
}

inline Op parse_op(std::string_view s) {
    if (s == "+")
        return Op::Add;
    if (s == "*")
        return Op::Mul;
    if (s == "-")
        return Op::Minus;

    if (auto op = magic_enum::enum_cast<Op>(s)) {
        return *op;
    }
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
        Op op = std::get<Op>(atom);
        if (op == Op::Add)
            return "+";
        if (op == Op::Mul)
            return "*";
        if (op == Op::Minus)
            return "-";

        return std::string(magic_enum::enum_name(op));
    } else if (std::holds_alternative<uint32_t>(atom)) {
        return get_string_from_lookup(std::get<uint32_t>(atom));
    } else if (std::holds_alternative<int>(atom)) {
        return std::to_string(std::get<int>(atom));
    } else if (std::holds_alternative<ScalarExpr>(atom)) {
        return std::get<ScalarExpr>(atom).to_string();
    }
    return "UnknownAtom";
}

inline Size bind_size(const Size &size, const SizeBindings *size_bindings) {
    if (!size_bindings) {
        return size;
    }

    if (const auto *symbol = std::get_if<std::string>(&size)) {
        if (auto it = size_bindings->find(*symbol); it != size_bindings->end()) {
            return it->second;
        }
    }

    return size;
}

inline Shape bind_shape(const Shape &shape, const SizeBindings *size_bindings) {
    return {bind_size(shape.first, size_bindings), bind_size(shape.second, size_bindings)};
}

inline Rewrite make_rewrite(
    const std::string &name, std::string_view lhs, std::string_view rhs, bool bidirectional = false,
    const std::function<bool(const EGraph &, const Substitution &)> &condition = nullptr,
    const std::function<std::pair<Id, bool>(EGraph &, const Substitution &, Id)> &applier = nullptr,
    size_t initial_match_limit = 30) {
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
    return egraph.add_node(ENode({}, register_string_in_lookup(identity_name)));
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

    return g.add_node(ENode({}, register_string_in_lookup(zero_name)));
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

inline const MatrixProperty *get_matrix_data(const EGraph &egraph, Id id) {
    return std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(id).property);
}

inline const TupleProperty *get_tuple_data(const EGraph &egraph, Id id) {
    return std::get_if<TupleProperty>(&egraph.get_class_analysis_data(id).property);
}

inline bool is_numeric(const Shape &shape) {
    return std::holds_alternative<int>(shape.first) && std::holds_alternative<int>(shape.second);
}

inline SizeBindings
sample_size_bindings(int lower_bound, int upper_bound, std::span<const std::string_view> keys, uint32_t seed = 42) {
    if (lower_bound > upper_bound) {
        throw std::invalid_argument("sample_size_bindings: lower_bound cannot be greater than upper_bound");
    }

    static thread_local std::mt19937 gen(seed);
    std::uniform_int_distribution<int> dist(lower_bound, upper_bound);

    SizeBindings bindings;
    bindings.reserve(keys.size());
    for (std::string_view key : keys) {
        bindings[std::string(key)] = dist(gen);
    }
    return bindings;
}

inline SizeBindings
sample_size_bindings(int lower_bound, int upper_bound, std::vector<std::string> keys, uint32_t seed = 42) {
    if (lower_bound > upper_bound) {
        throw std::invalid_argument("sample_size_bindings: lower_bound cannot be greater than upper_bound");
    }

    static thread_local std::mt19937 gen(seed);
    std::uniform_int_distribution<int> dist(lower_bound, upper_bound);

    SizeBindings bindings;
    bindings.reserve(keys.size());
    for (const std::string &key : keys) {
        bindings[key] = dist(gen);
    }
    return bindings;
}

inline std::vector<SizeBindings> sample_size_bindings(
    size_t k, int lower_bound, int upper_bound, const std::vector<std::string> &keys, uint32_t seed = 42,
    const PropertyTable *pt = nullptr) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> dist(lower_bound, upper_bound);
    std::vector<SizeBindings> samples;
    samples.reserve(k);
    for (size_t i = 0; i < k; ++i) {
        SizeBindings bindings;
        bindings.reserve(keys.size());
        for (const std::string &key : keys) {
            bindings[key] = dist(gen);
        }
        if (pt) {
            for (const auto &[name, mp] : pt->get_properties()) {
                if (mp.flags.is_tall) {
                    if (const auto *r_str = std::get_if<std::string>(&mp.shape.first)) {
                        if (const auto *c_str = std::get_if<std::string>(&mp.shape.second)) {
                            if (bindings.contains(*r_str) && bindings.contains(*c_str)) {
                                if (bindings[*r_str] <= bindings[*c_str]) {
                                    std::swap(bindings[*r_str], bindings[*c_str]);
                                    if (bindings[*r_str] == bindings[*c_str]) {
                                        bindings[*r_str] += 1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        samples.push_back(bindings);
    }
    return samples;
}

inline std::vector<double> generate_random_vector(int size) {
    std::vector<double> vec(size);
    std::mt19937 gen(99);
    std::uniform_real_distribution<double> dis(-1.0, 1.0);
    for (int i = 0; i < size; ++i) {
        vec[i] = dis(gen);
    }
    return vec;
}

inline std::vector<double> generate_identity_matrix(int size) {
    std::vector<double> vec(size * size, 0.0);
    for (int i = 0; i < size; ++i) {
        vec[i * size + i] = 1.0;
    }
    return vec;
}

inline std::optional<int> get_int_from_eclass(const EGraph &egraph, Id id) {
    for (const auto &node : egraph.get_class_nodes(id)) {
        Atom atom = node->get_atom();
        if (const int *i_val = std::get_if<int>(&atom)) {
            return *i_val;
        }
    }
    return std::nullopt;
}

inline std::optional<double> get_double_from_eclass(const EGraph &egraph, Id id) {
    for (const auto &node : egraph.get_class_nodes(id)) {
        Atom atom = node->get_atom();
        if (const ScalarExpr *s = std::get_if<ScalarExpr>(&atom)) {
            if (s->op == ScalarOp::Value) {
                return s->val;
            }
        }
    }
    return std::nullopt;
}
} // namespace egraph
