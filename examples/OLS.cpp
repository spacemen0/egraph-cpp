#include "MatrixIO.h"
#include "api.h"
#include "utils.h"
#include <iostream>
#include <utility>

int main() {
    EGraphRunner::Context ctx;
    ctx.get_config().enable_logging = true;
    Expression X = ctx.define_matrix_symbolic("X", "a", "b", {"full_rank", "tall"});
    Expression y = ctx.define_matrix_symbolic("y", "a", 1);
    Expression target_math = (inverse(transpose(X) * X) * transpose(X)) * y;

    ctx.optimize_symbolic(target_math);
    auto [x_sizes, x_data] = read_matrix("examples/data/ols_x.csv");
    auto [y_sizes, y_data] = read_matrix("examples/data/ols_y.csv");

    if (x_sizes.first <= 0 || x_sizes.second <= 0) {
        std::cerr << "Size error: X must be non-empty.\n";
        return 1;
    }
    if (y_sizes.second != 1) {
        std::cerr << "Size error: y must be a column vector.\n";
        return 1;
    }
    if (x_sizes.first != y_sizes.first) {
        std::cerr << "Size error: X and y must have the same number of rows.\n";
        return 1;
    }
    if (x_sizes.first <= x_sizes.second) {
        std::cerr << "Size error: X must be tall (rows > cols).\n";
        return 1;
    }

    SizeBindings concrete_sizes = {{"a", x_sizes.first}, {"b", x_sizes.second}};
    DataBindings concrete_data = {{"X", x_data}, {"y", y_data}};

    auto out = ctx.evaluate_concrete(concrete_sizes, concrete_data);
    auto out_shape = bind_shape(ctx.get_property().shape, &concrete_sizes);
    int row = std::get<int>(out_shape.first);
    int col = std::get<int>(out_shape.second);

    std::string out_path = "examples/data/ols_result.csv";
    write_matrix(out_path, row, col, out);
    std::cout << "OLS Evaluated result shape (" << row << "x" << col << ") saved to " << out_path << "\n";
    return 0;
}