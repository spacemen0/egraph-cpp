#pragma once

#include "errors.h"
#include "rewriter.h"
#include "types.h"
#include <charconv>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

inline Op parse_op(std::string_view s) {
    using enum Op;
    if (s == "Add")
        return Add;
    if (s == "Mul")
        return Mul;
    if (s == "Tr")
        return Tr;
    if (s == "Inv")
        return Inv;
    if (s == "Neg")
        return Neg;
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
    if (s == "TriSol")
        return TriSol;
    if (s == "Det")
        return Det;
    if (s == "Log")
        return Log;
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

inline ParsedAtom string_to_parsed_atom(std::string_view s) {
    s = trim(s);
    if (s.empty())
        throw ParseError("Empty string");

    auto pos = s.find('(');

    // Leaf node
    if (pos == std::string_view::npos) {
        int v;
        if (auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v); ec == std::errc()) {
            if (ptr == s.data() + s.size()) // the entire string was consumed
                return {v, {}};
            throw ParseError("Invalid integer: " + std::string(s));
        }
        return {std::string(s), {}};
    }

    std::string_view op_str = trim(s.substr(0, pos));
    Op op = parse_op(op_str);

    size_t end = s.find_last_of(')');
    if (end == std::string_view::npos)
        throw ParseError("Missing closing parenthesis");

    std::string_view args_str = s.substr(pos + 1, end - (pos + 1));
    std::vector<std::string> children;

    size_t child_start = 0;
    int paren_count = 0;
    for (size_t i = 0; i < args_str.size(); ++i) {
        if (args_str[i] == '(')
            paren_count++;
        else if (args_str[i] == ')')
            paren_count--;
        else if (args_str[i] == ',' && paren_count == 0) {

            if (auto child = trim(args_str.substr(child_start, i - child_start)); !child.empty())
                children.emplace_back(child);
            child_start = i + 1;
        }
    }
    if (child_start < args_str.size()) {
        auto child = trim(args_str.substr(child_start));
        if (!child.empty())
            children.emplace_back(child);
    }
    return {op, children};
}

inline std::string atom_to_string(const Atom &atom) {
    if (std::holds_alternative<Op>(atom)) {
        switch (std::get<Op>(atom)) {
            using enum Op;
        case Add:
            return "Add";
        case Mul:
            return "Mul";
        case Tr:
            return "Tr";
        case Inv:
            return "Inv";
        case Neg:
            return "Neg";
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
        case TriSol:
            return "TriSol";
        case Det:
            return "Det";
        case Log:
            return "Log";
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
    const std::string &name, std::string_view lhs, std::string_view rhs,
    const std::function<bool(const EGraph &, const Substitution &)> &condition = nullptr,
    const std::function<Id(EGraph &, const Substitution &)> &applier = nullptr,
    size_t initial_match_limit = std::numeric_limits<size_t>::max()) {
    return Rewrite{name, Pattern(lhs), Pattern(rhs), condition, applier, initial_match_limit};
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
        .is_symmetric = true,
        .is_orthogonal = true,
        .is_identity = true,
        .is_zero = false,
        .is_upper_triangular = true,
        .is_lower_triangular = true,
        .is_diagonal = true,
        .is_singular = false};

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

inline Id make_zero_for(EGraph &g, const Substitution &s, const std::string &var_name) {
    Id id = s.at(var_name);
    const auto &data = g.get_class_analysis_data(id);
    const auto *matrix_prop_ptr = std::get_if<MatrixProperty>(&data.property);
    if (!matrix_prop_ptr) {
        throw std::runtime_error("make_zero_for: Expected MatrixProperty but got TupleProperty");
    }
    const auto &matrix_prop = *matrix_prop_ptr;
    auto shape = matrix_prop.shape;

    MatrixProperty prop;
    prop.shape = shape;
    prop.flags = {
        .is_symmetric = true,
        .is_identity = false,
        .is_zero = true,
        .is_upper_triangular = true,
        .is_lower_triangular = true,
        .is_diagonal = true,
        .is_singular = true,
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