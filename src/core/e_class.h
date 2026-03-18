#pragma once

#include <vector>
#include "types.h"
#include "e_node.h"

class EClass
{
public:
    explicit EClass(Id id, const ENode *initial_node, AnalysisData data)
        : _id(id), analysis_data(std::move(data))
    {
        nodes.push_back(initial_node);
    }
    std::vector<const ENode *> &get_nodes();
    std::vector<Id> &get_parents();
    AnalysisData &get_analysis_data();
    void clean_up_nodes();

private:
    Id _id;
    std::vector<const ENode *> nodes;
    // parents are Enodes pointing to this class
    std::vector<Id> parents;
    AnalysisData analysis_data;
};