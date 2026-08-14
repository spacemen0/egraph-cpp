#pragma once

#include "condition_guards.h"
#include "e_graph.h"
#include "property_table.h"
#include "utils.h"
#include <string>
#include <string_view>
#include <variant>

static const auto transpose_spd = make_rewrite(
    "transpose_spd", "?a * Tr(?a)", "Dynamic", false, is_full_rank("a"),

    [](EGraph &g, const Substitution &s, Id class_id) {
    Id a_id = s.at("a");
    const auto *a_prop = get_matrix_data(g, a_id);
    auto old_data = g.get_class_analysis_data(class_id);
    if (auto *mp = std::get_if<MatrixProperty>(&old_data.property)) {
        mp->flags.is_positive_definite = true;
        mp->flags.is_symmetric = true;
    };
    return std::make_pair(class_id, g.update_class_analysis_data(class_id, old_data));
});

static const auto transpose_spd2 = make_rewrite(
    "transpose_spd2", "Tr(?a) * ?a", "Dynamic", false, is_full_rank("a"),

    [](EGraph &g, const Substitution &s, Id class_id) {
    Id a_id = s.at("a");
    const auto *a_prop = get_matrix_data(g, a_id);
    auto old_data = g.get_class_analysis_data(class_id);
    if (auto *mp = std::get_if<MatrixProperty>(&old_data.property)) {
        mp->flags.is_positive_definite = true;
        mp->flags.is_symmetric = true;
    };
    return std::make_pair(class_id, g.update_class_analysis_data(class_id, old_data));
});

static const auto sandwich_spd_left = make_rewrite(
    "sandwich_spd_left", "Tr(?a) * (?b * ?a)", "Dynamic", false,
    [](const EGraph &g, const Substitution &s) {
        return is_full_rank("a")(g, s) && (is_pos_def("b")(g, s) || is_symmetric("b")(g, s));
    },
    [](EGraph &g, const Substitution &s, Id class_id) {
        Id b_id = s.at("b");
        const auto *b_prop = get_matrix_data(g, b_id);
        auto old_data = g.get_class_analysis_data(class_id);
        if (auto *mp = std::get_if<MatrixProperty>(&old_data.property)) {
            mp->flags.is_symmetric = true;
            if (b_prop && b_prop->flags.is_positive_definite) {
                mp->flags.is_positive_definite = true;
            }
        }
        return std::make_pair(class_id, g.update_class_analysis_data(class_id, old_data));
    });

static const auto sandwich_spd_right = make_rewrite(
    "sandwich_spd_right", "?a * (?b * Tr(?a))", "Dynamic", false,
    [](const EGraph &g, const Substitution &s) {
        return is_full_rank("a")(g, s) && (is_pos_def("b")(g, s) || is_symmetric("b")(g, s));
    },
    [](EGraph &g, const Substitution &s, Id class_id) {
        Id b_id = s.at("b");
        const auto *b_prop = get_matrix_data(g, b_id);
        auto old_data = g.get_class_analysis_data(class_id);
        if (auto *mp = std::get_if<MatrixProperty>(&old_data.property)) {
            mp->flags.is_symmetric = true;
            if (b_prop && b_prop->flags.is_positive_definite) {
                mp->flags.is_positive_definite = true;
            }
        }
        return std::make_pair(class_id, g.update_class_analysis_data(class_id, old_data));
    });

static const std::vector<Rewrite> property_discovery_set = {
    transpose_spd,
    transpose_spd2,
    sandwich_spd_left,
    sandwich_spd_right,
};