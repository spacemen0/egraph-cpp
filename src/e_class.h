#pragma once
#include <vector>
#include "types.h"
#include "e_node.h"

class EClass
{
public:
    Id id;
    EClass(Id id) : id(id) {}
    std::vector<ENode> &get_nodes();
    std::vector<Id> &get_parents();

private:
    std::vector<ENode> nodes;
    std::vector<Id> parents;
};