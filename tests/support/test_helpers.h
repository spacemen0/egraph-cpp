#pragma once
#include "e_graph.h"
#include "e_node.h"
#include "extractor.h"
#include "property_table.h"
#include <gtest/gtest.h>

static ENode make_symbol(const std::string &name) { return ENode(Children{}, register_string_in_lookup(name)); }

static ENode make_leaf(Op op) { return ENode(Children{}, op); }

static ENode make_op(Op op, const Children &children) { return ENode(children, op); }

static PropertyTable get_property_table() {
    PropertyTable pt;
    pt.add_or_update_property_entry("A", {.shape = std::make_pair(3, 3), .flags = {.is_non_singular = true}});
    pt.add_or_update_property_entry("B", {.shape = std::make_pair(2, 4), .flags = {.is_full_rank = true}});
    pt.add_or_update_property_entry("C", {.shape = std::make_pair(4, 2), .flags = {.is_full_rank = true}});
    pt.add_or_update_property_entry("D", {.shape = std::make_pair(2, 2), .flags = {.is_non_singular = true}});
    pt.add_or_update_property_entry("X", {.shape = std::make_pair(3, 2), .flags = {.is_full_rank = true}});
    pt.add_or_update_property_entry("Y", {.shape = std::make_pair(2, 3), .flags = {.is_full_rank = true}});
    pt.add_or_update_property_entry("J", {.shape = std::make_pair(30, 20), .flags = {.is_full_rank = true}});
    pt.add_or_update_property_entry("Z", {.shape = std::make_pair(3, 3), .flags = {.is_non_singular = true}});
    pt.add_or_update_property_entry("W", {.shape = std::make_pair(2, 2), .flags = {.is_non_singular = true}});
    pt.add_or_update_property_entry(
        "V", {.shape = std::make_pair(3, 3), .flags = {.is_symmetric = true, .is_positive_definite = true}});
    pt.add_or_update_property_entry("I_3x3", {.shape = std::make_pair(3, 3), .flags = {.is_identity = true}});
    pt.add_or_update_property_entry("Zero", {.shape = std::make_pair(3, 3), .flags = {.is_zero = true}});
    pt.add_or_update_property_entry("y", {.shape = std::make_pair(3, 1)});
    pt.add_or_update_property_entry("k", {.shape = std::make_pair(30, 1)});
    pt.add_or_update_property_entry(
        "M", {.shape = std::make_pair("A", "B"), .flags = {.is_full_rank = true, .is_tall = true}});
    pt.add_or_update_property_entry("n", {.shape = std::make_pair("A", 1)});
    pt.add_or_update_property_entry(
        "v", {.shape = std::make_pair("A", "A"), .flags = {.is_symmetric = true, .is_positive_definite = true}});
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
    pt.add_or_update_property_entry("G", {.shape = std::make_pair("g", "h")});
    return pt;
}

class EGraphTest : public ::testing::Test {
  protected:
    EGraph egraph{get_property_table()};
};

class ExtractorTest : public EGraphTest {
  protected:
    CostStorage cost_storage{egraph};
    Extractor extractor{egraph, cost_storage};
};

const static ENode sym_a = make_symbol("A");
const static ENode sym_b = make_symbol("B");
const static ENode sym_c = make_symbol("C");
const static ENode sym_d = make_symbol("D");
const static ENode sym_x = make_symbol("X");
const static ENode sym_y = make_symbol("Y");
const static ENode sym_z = make_symbol("Z");
const static ENode sym_w = make_symbol("W");