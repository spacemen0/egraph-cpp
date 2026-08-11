#include "ReadMatrix.h"
#include "api.h"
#include "utils.h"
#include <iostream>
#include <utility>

int main() {
    EGraphRunner::Context ctx;

    Expression X = ctx.define_matrix_symbolic("X", "a", "b", {"full_rank", "tall"});
    Expression n = ctx.define_matrix_symbolic("n", "a", 1);
    Expression target_math = (inverse(transpose(X) * X) * transpose(X)) * n;

    ctx.optimize_symbolic(target_math);
    auto [x_sizes, x_data] = read_matrix("data/ols_x.txt");
    auto [_, y_data] = read_matrix("data/ols_y.txt");
    SizeBindings concrete_sizes = {{"a", x_sizes.first}, {"b", x_sizes.second}};
    DataBindings concrete_data = {{"X", x_data}, {"y", y_data}};

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
    return 0;
}