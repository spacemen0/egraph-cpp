#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

/**
 * Acts like MATLAB's readmatrix().
 * Reads a text file containing space or comma-separated numbers.
 * Automatically infers rows and columns.
 * 
 * NOTE: Converts the data from row-major (how it's read from the file) 
 * to column-major format (which is required by the engine's DataBindings).
 * 
 * @param filename Path to the txt/csv file
 * @return std::pair containing:
 *         - pair<int, int>: {rows, cols}
 *         - vector<double>: the data in column-major order
 */
inline std::pair<std::pair<int, int>, std::vector<double>> read_matrix(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::vector<double> row_major_data;
    std::string line;
    int rows = 0;
    int cols = 0;

    while (std::getline(file, line)) {
        // Skip empty lines
        if (line.empty()) continue;

        // Replace any commas with spaces to handle CSVs
        for (char& c : line) {
            if (c == ',') c = ' ';
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
