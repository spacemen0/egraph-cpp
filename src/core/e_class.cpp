#include "e_class.h"
#include <algorithm>

std::vector<const ENode *> &EClass::get_nodes() { return nodes; }

std::vector<Id> &EClass::get_parents() { return parents; }

AnalysisData &EClass::get_analysis_data() { return analysis_data; }

// remove duplicates
void EClass::clean_up_nodes() {
    std::sort(nodes.begin(), nodes.end(), [](const ENode *a, const ENode *b) {
        if (a->get_atom() != b->get_atom()) {
            return a->get_atom() < b->get_atom();
        }
        return a->get_children() < b->get_children();
    });

    auto last = std::unique(nodes.begin(), nodes.end(), [](const ENode *a, const ENode *b) {
        return *a == *b;
    });

    nodes.erase(last, nodes.end());
}
