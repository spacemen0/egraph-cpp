#pragma once
#include "e_graph.h"
#include "e_node.h"
#include "extractor.h"
#include "property_table.h"
#include "utils.h"
#include <fstream>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace egraph;

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
    Extractor extractor{egraph};
};

const static ENode sym_a = make_symbol("A");
const static ENode sym_b = make_symbol("B");
const static ENode sym_c = make_symbol("C");
const static ENode sym_d = make_symbol("D");
const static ENode sym_x = make_symbol("X");
const static ENode sym_y = make_symbol("Y");
const static ENode sym_z = make_symbol("Z");
const static ENode sym_w = make_symbol("W");

/**
 * Acts like MATLAB's readmatrix().
 * Reads a CSV or text file containing space or comma-separated numbers.
 * Automatically infers rows and columns.
 *
 * NOTE: Converts the data from row-major (file format)
 * to column-major format (required by the engine's DataBindings/Evaluator).
 *
 * @param filename Path to the csv/txt file
 * @return std::pair containing:
 *         - pair<int, int>: {rows, cols}
 *         - vector<double>: the data in column-major order
 */
inline std::pair<std::pair<int, int>, std::vector<double>> read_matrix(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for reading: " + filename);
    }

    std::vector<double> row_major_data;
    std::string line;
    int rows = 0;
    int cols = 0;

    while (std::getline(file, line)) {
        if (line.empty())
            continue;

        for (char &c : line) {
            if (c == ',')
                c = ' ';
        }

        std::stringstream ss(line);
        double val;
        int current_cols = 0;

        while (ss >> val) {
            row_major_data.push_back(val);
            current_cols++;
        }

        if (current_cols > 0) {
            if (rows == 0) {
                cols = current_cols;
            } else if (current_cols != cols) {
                throw std::runtime_error("Inconsistent number of columns at row " + std::to_string(rows));
            }
            rows++;
        }
    }

    // Convert from row-major (file format) to column-major (engine format)
    std::vector<double> col_major_data(rows * cols);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            col_major_data[r + c * rows] = row_major_data[r * cols + c];
        }
    }

    return {{rows, cols}, col_major_data};
}

/**
 * Acts like MATLAB's writematrix().
 * Writes a column-major matrix buffer (engine format) to a comma-separated CSV file.
 *
 * @param filename Path to the output CSV file
 * @param rows Number of rows
 * @param cols Number of columns
 * @param col_major_data 1D vector containing matrix data in column-major order
 * @param precision Number of decimal places for formatting (default: 6)
 */
inline void write_matrix(
    const std::string &filename, int rows, int cols, const std::vector<double> &col_major_data, int precision = 17) {
    if (static_cast<size_t>(rows * cols) > col_major_data.size()) {
        throw std::invalid_argument(
            "Data buffer size (" + std::to_string(col_major_data.size()) + ") smaller than matrix dimensions (" +
            std::to_string(rows) + "x" + std::to_string(cols) + ")");
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + filename);
    }

    file << std::setprecision(precision);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            file << col_major_data[r + c * rows];
            if (c + 1 < cols) {
                file << ",";
            }
        }
        file << "\n";
    }
}
