#pragma once

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    const std::string &filename, int rows, int cols, const std::vector<double> &col_major_data, int precision = 6) {
    if (static_cast<size_t>(rows * cols) > col_major_data.size()) {
        throw std::invalid_argument(
            "Data buffer size (" + std::to_string(col_major_data.size()) + ") smaller than matrix dimensions (" +
            std::to_string(rows) + "x" + std::to_string(cols) + ")");
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for writing: " + filename);
    }

    file << std::fixed << std::setprecision(precision);

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
