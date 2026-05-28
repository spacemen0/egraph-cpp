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