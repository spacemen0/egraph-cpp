#pragma once

#include "e_graph.h"
#include "utils.h"
#include <string>
#include <string_view>

namespace egraph {
static auto is_leaf = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        Id id = s.at(var);
        return g.at(id).get_children().empty();
    };
};

static auto is_not_vector = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        Id a_id = s.at(var);
        const auto &data = g.get_class_analysis_data(a_id);
        if (const auto *prop = std::get_if<MatrixProperty>(&data.property)) {
            return !prop->is_vector();
        }
        return true;
    };
};

static auto is_not_transpose = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        Id id = s.at(var);
        for (const auto *node : g.get_class_nodes(id)) {
            if (std::holds_alternative<Op>(node->get_atom()) && std::get<Op>(node->get_atom()) == Op::Tr) {
                return false;
            }
        }
        return true;
    };
};

static auto is_not_factorized = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        Id id = s.at(var);
        const auto &data = g.get_class_analysis_data(id);
        if (const auto *prop = std::get_if<MatrixProperty>(&data.property)) {
            return !(
                prop->flags.is_upper_triangular || prop->flags.is_lower_triangular || prop->flags.is_diagonal ||
                prop->flags.is_identity || prop->flags.is_zero || prop->flags.is_orthogonal ||
                prop->flags.has_orthonormal_columns);
        }
        return true;
    };
};
static auto is_square = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        Id id = s.at(var);
        if (auto prop = std::get_if<MatrixProperty>(&g.get_class_analysis_data(id).property))
            return prop->is_square();
        return false;
    };
};

static auto is_identity_cond = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        Id id = s.at(var);
        if (auto prop = std::get_if<MatrixProperty>(&g.get_class_analysis_data(id).property))
            return prop->flags.is_identity;
        return false;
    };
};

static auto is_zero_cond = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        Id id = s.at(var);
        if (auto prop = std::get_if<MatrixProperty>(&g.get_class_analysis_data(id).property))
            return prop->flags.is_zero;
        return false;
    };
};

static auto is_pos_def = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        const auto *prop = get_matrix_data(g, s.at(var));
        return prop && prop->flags.is_positive_definite;
    };
};

static auto is_symmetric = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        const auto *prop = get_matrix_data(g, s.at(var));
        return prop && prop->flags.is_symmetric;
    };
};

static auto is_non_singular_cond = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        const auto *prop = get_matrix_data(g, s.at(var));
        return prop && prop->is_square() && prop->flags.is_non_singular;
    };
};

static auto is_orthogonal_cond = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        const auto *prop = get_matrix_data(g, s.at(var));
        return prop && prop->flags.is_orthogonal;
    };
};

static auto is_orthonormal_cond = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        const auto *prop = get_matrix_data(g, s.at(var));
        return prop && prop->flags.has_orthonormal_columns;
    };
};

static auto is_triangular = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        const auto *prop = get_matrix_data(g, s.at(var));
        return prop && (prop->flags.is_upper_triangular || prop->flags.is_lower_triangular);
    };
};

static auto is_matrix = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        const auto *prop = get_matrix_data(g, s.at(var));
        return prop && !prop->is_vector() && !prop->is_scalar();
    };
};

static auto is_full_rank = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        const auto *prop = get_matrix_data(g, s.at(var));
        return prop && prop->flags.is_full_rank;
    };
};

static auto is_full_column_rank = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        const auto *prop = get_matrix_data(g, s.at(var));
        return prop && prop->flags.is_full_rank && (prop->is_square() || prop->is_tall_matrix());
    };
};

static auto is_full_row_rank = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        const auto *prop = get_matrix_data(g, s.at(var));
        return prop && prop->flags.is_full_rank && (prop->is_square() || prop->is_wide_matrix());
    };
};

static auto is_vector = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        const auto *prop = get_matrix_data(g, s.at(var));
        return prop && prop->is_vector();
    };
};

static auto leaf_and_not_factorized = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        return is_leaf(var)(g, s) && is_not_factorized(var)(g, s);
    };
};

static auto leaf_and_not_factorized_and_square = [](std::string_view var) {
    return [var = std::string(var)](const EGraph &g, const Substitution &s) {
        return is_leaf(var)(g, s) && is_not_factorized(var)(g, s) && is_square(var)(g, s);
    };
};

static auto is_not_op = [](std::string_view var, Op op) {
    return [var = std::string(var), op](const EGraph &g, const Substitution &s) {
        Id id = s.at(var);
        const auto &node = g.at(id);
        return !std::holds_alternative<Op>(node.get_atom()) || std::get<Op>(node.get_atom()) != op;
    };
};
} // namespace egraph
