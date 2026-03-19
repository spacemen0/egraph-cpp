#pragma once
#include "e_node.h"
#include "property_table.h"

static ENode make_symbol(const std::string &name) { return ENode(Children{}, std::string(name)); }

static ENode make_leaf(Op op) { return ENode(Children{}, op); }

static ENode make_op(Op op, const Children &children) { return ENode(children, op); }

static PropertyTable get_property_table() {
    PropertyTable pt;
    pt.add_or_update_property_entry("A", {.shape = std::make_pair(3, 3)});
    pt.add_or_update_property_entry("B", {.shape = std::make_pair(2, 4)});
    pt.add_or_update_property_entry("C", {.shape = std::make_pair(4, 2)});
    pt.add_or_update_property_entry("D", {.shape = std::make_pair(2, 2)});
    pt.add_or_update_property_entry("X", {.shape = std::make_pair(3, 2), .is_positive_definite = true});
    pt.add_or_update_property_entry("Y", {.shape = std::make_pair(2, 3)});
    pt.add_or_update_property_entry("Z", {.shape = std::make_pair(3, 3)});
    pt.add_or_update_property_entry("W", {.shape = std::make_pair(2, 2)});
    pt.add_or_update_property_entry(
        "V", {.shape = std::make_pair(3, 3), .is_symmetric = true, .is_positive_definite = true});
    pt.add_or_update_property_entry("I_3x3", {.shape = std::make_pair(3, 3), .is_identity = true});
    pt.add_or_update_property_entry("Zero", {.shape = std::make_pair(3, 3), .is_zero = true});
    pt.add_or_update_property_entry("y", {.shape = std::make_pair(3, 1)});
    pt.add_or_update_property_entry(
        "M", {.shape = std::make_pair("A", "B"), .is_positive_definite = true, .is_tall = true});
    pt.add_or_update_property_entry("n", {.shape = std::make_pair("A", 1)});
    pt.add_or_update_property_entry(
        "v", {.shape = std::make_pair("A", "A"), .is_symmetric = true, .is_positive_definite = true});

    return pt;
}

static PropertyTable get_property_table_with_symbolic_shapes() {
    PropertyTable pt;
    pt.add_or_update_property_entry("A", {.shape = std::make_pair("a", "b")});
    pt.add_or_update_property_entry("B", {.shape = std::make_pair("b", "c")});
    pt.add_or_update_property_entry("C", {.shape = std::make_pair("c", "d")});
    pt.add_or_update_property_entry("D", {.shape = std::make_pair("d", "e")});
    pt.add_or_update_property_entry("E", {.shape = std::make_pair("e", "f")});
    pt.add_or_update_property_entry("F", {.shape = std::make_pair("f", "g")});
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