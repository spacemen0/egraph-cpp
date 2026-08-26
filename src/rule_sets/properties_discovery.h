#pragma once

#include "condition_guards.h"
#include "e_graph.h"
#include "property_table.h"
#include "utils.h"
#include <string>
#include <string_view>
#include <variant>


namespace egraph {
static const auto transpose_spd = make_rewrite(
    "transpose_spd", "?a * Tr(?a)", "Dynamic", false, is_full_row_rank("a"),

    [](EGraph &g, const Substitution &s, Id class_id) {
    auto old_data = g.get_class_analysis_data(class_id);
    if (auto *mp = std::get_if<MatrixProperty>(&old_data.property)) {
        mp->flags.is_positive_definite = true;
        mp->flags.is_symmetric = true;
    };
    return std::make_pair(class_id, g.update_class_analysis_data(class_id, old_data));
});

static const auto transpose_spd2 = make_rewrite(
    "transpose_spd2", "Tr(?a) * ?a", "Dynamic", false, is_full_column_rank("a"),

    [](EGraph &g, const Substitution &s, Id class_id) {
    auto old_data = g.get_class_analysis_data(class_id);
    if (auto *mp = std::get_if<MatrixProperty>(&old_data.property)) {
        mp->flags.is_positive_definite = true;
        mp->flags.is_symmetric = true;
    };
    return std::make_pair(class_id, g.update_class_analysis_data(class_id, old_data));
});

static const auto sandwich_spd_left = make_rewrite(
    "sandwich_spd_left", "Tr(?a) * (?b * ?a)", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_full_column_rank("a")(g, s) && is_symmetric("b")(g, s);
}, [](EGraph &g, const Substitution &s, Id class_id) {
    Id b_id = s.at("b");
    const auto *b_prop = get_matrix_data(g, b_id);
    auto old_data = g.get_class_analysis_data(class_id);
    if (b_prop) {
        if (auto *mp = std::get_if<MatrixProperty>(&old_data.property)) {
            mp->flags.is_symmetric = true;
            if (b_prop->flags.is_positive_definite) {
                mp->flags.is_positive_definite = true;
            }
        }

        return std::make_pair(class_id, g.update_class_analysis_data(class_id, old_data));
    }
    throw InvalidOperationError("sandwich_spd_left: b must be a matrix with properties");
});

static const auto sandwich_spd_right = make_rewrite(
    "sandwich_spd_right", "?a * (?b * Tr(?a))", "Dynamic", false, [](const EGraph &g, const Substitution &s) {
    return is_full_row_rank("a")(g, s) && is_symmetric("b")(g, s);
}, [](EGraph &g, const Substitution &s, Id class_id) {
    Id b_id = s.at("b");
    const auto *b_prop = get_matrix_data(g, b_id);
    auto old_data = g.get_class_analysis_data(class_id);
    if (b_prop) {
        if (auto *mp = std::get_if<MatrixProperty>(&old_data.property)) {
            mp->flags.is_symmetric = true;
            if (b_prop->flags.is_positive_definite) {
                mp->flags.is_positive_definite = true;
            }
        }
        return std::make_pair(class_id, g.update_class_analysis_data(class_id, old_data));
    }
    throw InvalidOperationError("sandwich_spd_right: b must be a matrix with properties");
});

static const std::vector<Rewrite> property_discovery_set = {
    transpose_spd,
    transpose_spd2,
    sandwich_spd_left,
    sandwich_spd_right,
};
} // namespace egraph
