#pragma once
#include <vector>
#include "types.h"

class EClass
{
public:
    Id id;

    const std::vector<Id> &get_nodes() const;
    const std::vector<Id> &get_parents() const;

private:
    std::vector<Id> nodes;
    std::vector<Id> parents;
};