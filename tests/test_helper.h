#pragma once
#include "e_node.h"
#include "property_table.h"

static ENode make_symbol(const std::string &name)
{
    return ENode(Children{}, std::string(name));
}

static ENode make_leaf(Op op)
{
    return ENode(Children{}, op);
}

static ENode make_op(Op op, const Children &children)
{
    return ENode(children, op);
}

static PropertyTable get_property_table()
{
    PropertyTable pt;
    pt.add_property_entry("A", {std::make_pair(3, 3), true, true});
    pt.add_property_entry("B", {std::make_pair(2, 4)});
    pt.add_property_entry("C", {std::make_pair(4, 2)});
    pt.add_property_entry("D", {std::make_pair(2, 2)});
    pt.add_property_entry("X", {std::make_pair(3, 2)});
    pt.add_property_entry("Y", {std::make_pair(2, 3)});
    pt.add_property_entry("Z", {std::make_pair(3, 3)});
    pt.add_property_entry("W", {std::make_pair(2, 2)});
    pt.add_property_entry("I_3x3", {std::make_pair(3, 3), true, true, true, false});
    pt.add_property_entry("Zero", {std::make_pair(3, 3), true, false, false, true});

    return pt;
}
const static ENode sym_a = make_symbol("A");
const static ENode sym_b = make_symbol("B");
const static ENode sym_c = make_symbol("C");
const static ENode sym_d = make_symbol("D");
const static ENode sym_x = make_symbol("X");
const static ENode sym_y = make_symbol("Y");
const static ENode sym_z = make_symbol("Z");
const static ENode sym_w = make_symbol("W");