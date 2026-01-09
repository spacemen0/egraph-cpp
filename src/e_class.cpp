#include "e_class.h"

std::vector<const ENode *> &EClass::get_nodes()
{
    return nodes;
}

std::vector<Id> &EClass::get_parents()
{
    return parents;
}

AnalysisData &EClass::get_analysis_data()
{
    return analysis_data;
}
