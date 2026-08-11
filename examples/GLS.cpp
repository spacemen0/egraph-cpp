#include "MatrixIO.h"
#include "api.h"
#include <iostream>

int main() {
    EGraphRunner::Context ctx;

    Expression X = ctx.define_matrix_symbolic("X", "a", "b", {"full_rank", "tall"});
    Expression y = ctx.define_matrix_symbolic("y", "a", 1);
    Expression M = ctx.define_matrix_symbolic("M", "a", "a", {"symmetric", "positive_definite"});
    Expression target_math = inverse(transpose(X) * inverse(M) * X) * transpose(X) * inverse(M) * y;

    ctx.optimize_symbolic(target_math);
    auto [m_sizes, m_data] = read_matrix("examples/data/gls_m.csv");
    auto [x_sizes, x_data] = read_matrix("examples/data/gls_x.csv");
    auto [y_sizes, y_data] = read_matrix("examples/data/gls_y.csv");

    if (m_sizes.first != m_sizes.second) {
        std::cerr << "Size error: M must be square.\n";
        return 1;
    }
    if (y_sizes.second != 1) {
        std::cerr << "Size error: y must be column vectors.\n";
        return 1;
    }
    if (x_sizes.first != y_sizes.first) {
        std::cerr << "Size error: X and y must have the same number of rows.\n";
        return 1;
    }
    if (m_sizes.first != x_sizes.first) {
        std::cerr << "Size error: M and X must have the same number of rows.\n";
        return 1;
    }

    SizeBindings concrete_sizes = {{"a", x_sizes.first}, {"b", x_sizes.second}};
    DataBindings concrete_data = {{"X", x_data}, {"M", m_data}, {"y", y_data}};

    auto out = ctx.evaluate_concrete(concrete_sizes, concrete_data);
    auto out_shape = bind_shape(ctx.get_property().shape, &concrete_sizes);
    int row = std::get<int>(out_shape.first);
    int col = std::get<int>(out_shape.second);

    std::string out_path = "examples/data/gls_result.csv";
    write_matrix(out_path, row, col, out);
    return 0;
}