#include "e_class.h"

const std::vector<Id> &EClass::get_nodes() const
{
    return nodes;
}

const std::vector<Id> &EClass::get_parents() const
{
    return parents;
}
