#pragma once
#include <vector>
#include "types.h"
#include "e_node.h"

class EClass
{
public:
    explicit EClass(Id id) : id(id) {}
    std::vector<const ENode *> &get_nodes();
    std::vector<Id> &get_parents();

private:
    Id id;
    std::vector<const ENode *> nodes;
    std::vector<Id> parents;
};