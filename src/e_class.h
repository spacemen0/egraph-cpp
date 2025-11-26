#pragma once
#include <vector>
#include "types.h"
#include "e_node.h"

class EClass
{
public:
    explicit EClass(Id id) : _id(id) {}
    std::vector<const ENode *> &get_nodes();
    std::vector<Id> &get_parents();

private:
    Id _id;
    std::vector<const ENode *> nodes;
    // parents are Enodes pointing to this class
    std::vector<Id> parents;
};