#include "MatrixIO.h"
#include "examples.h"
#include <chrono>
#include <iostream>
#include <lapacke.h>
#include <vector>

int run_ols_dgels() {
    std::cout << "=== Running OLS Direct LAPACK dgels Benchmark Example ===\n";
    auto [x_sizes, x_data] = read_matrix("examples/data/ols_x.csv");
    auto [y_sizes, y_data] = read_matrix("examples/data/ols_y.csv");

    int m = x_sizes.first;
    int n = x_sizes.second;
    int nrhs = y_sizes.second;

    if (m <= 0 || n <= 0 || y_sizes.second != 1 || m != y_sizes.first) {
        std::cerr << "Invalid matrix dimensions for OLS.\n";
        return 1;
    }

    std::vector<double> a_copy = x_data;
    std::vector<double> b_copy = y_data;

    auto start = std::chrono::high_resolution_clock::now();

    int info = LAPACKE_dgels(LAPACK_COL_MAJOR, 'N', m, n, nrhs, a_copy.data(), m, b_copy.data(), m);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    if (info != 0) {
        std::cerr << "LAPACKE_dgels failed with info = " << info << "\n";
        return 1;
    }

    std::vector<double> beta(b_copy.begin(), b_copy.begin() + n);

    std::cout << "[dgels Direct] Evaluation time: " << duration << " microseconds\n";

    std::string out_path = "examples/data/ols_dgels_result.csv";
    write_matrix(out_path, n, 1, beta);
    std::cout << "OLS dgels result shape (" << n << "x1) saved to " << out_path << "\n\n";

    return 0;
}
