#include "expansions.h"
#include "lowering.h"
#include "rewriter.h"
#include "simplification.h"
#include "transformation.h"
#include <string>
#include <vector>

static std::vector<Rewrite> build_complete_rewrite_set() {
    std::vector<Rewrite> rewrites;
    rewrites.insert(rewrites.end(), simplification_set.begin(), simplification_set.end());
    rewrites.insert(rewrites.end(), transformation_set.begin(), transformation_set.end());
    rewrites.insert(rewrites.end(), expansion_set.begin(), expansion_set.end());
    rewrites.insert(rewrites.end(), lowering_set.begin(), lowering_set.end());
    return rewrites;
}

inline std::vector<Rewrite> get_rewrite_set_by_name(const std::string &name) {
    if (name == "complete") {
        return build_complete_rewrite_set();
    }
    if (name == "simplification") {
        return simplification_set;
    }
    if (name == "transformation") {
        return transformation_set;
    }
    if (name == "expansion") {
        return expansion_set;
    }
    if (name == "lowering") {
        return lowering_set;
    }
    throw std::invalid_argument("Unknown rewrite set name: " + name);
}

inline std::vector<Rewrite> build_rewrite_sets(const std::vector<std::string> &set_names) {
    std::vector<Rewrite> rules;
    for (const auto &set_name : set_names) {
        auto set_rules = get_rewrite_set_by_name(std::string(set_name));
        rules.insert(rules.end(), set_rules.begin(), set_rules.end());
    }
    return rules;
}
