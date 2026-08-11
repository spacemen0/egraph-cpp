#include "ReadMatrix.h"
#include "api.h"
#include <iostream>

int main() {
    EGraphRunner::Context ctx;

    Expression X = ctx.define_matrix_symbolic("X", "a", "b", {"full_rank", "tall"});
    Expression y = ctx.define_matrix_symbolic("y", "a", 1);
    Expression M = ctx.define_matrix_symbolic("M", "a", "a", {"symmetric", "positive_definite"});
    Expression target_math = inverse(transpose(X) * inverse(M) * X) * transpose(X) * inverse(M) * y;

    ctx.optimize_symbolic(target_math);
    auto [m_sizes, m_data] = read_matrix("data/gls_m.txt");
    auto [x_sizes, x_data] = read_matrix("data/gls_x.txt");
    auto [y_sizes, y_data] = read_matrix("data/gls_y.txt");

    SizeBindings concrete_sizes = {{"a", x_sizes.first}, {"b", x_sizes.second}};
    DataBindings concrete_data = {{"X", x_data}, {"M", m_data}, {"y", y_data}};

    auto out = ctx.evaluate_concrete(concrete_sizes, concrete_data);
    auto out_shape = bind_shape(ctx.get_property().shape, &concrete_sizes);
    int row = std::get<int>(out_shape.first);
    int col = std::get<int>(out_shape.second);
    for (int i = 0; i < row; ++i) {
        for (int j = 0; j < col; ++j) {
            std::cout << out[i * col + j] << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}