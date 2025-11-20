#include "e_class.h"

std::vector<ENode> &EClass::get_nodes()
{
    return nodes;
}

std::vector<Id> &EClass::get_parents()
{
    return parents;
}
