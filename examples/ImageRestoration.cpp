#include "ReadMatrix.h"
#include "api.h"
#include <iostream>
#include <vector>

int main() {
    EGraphRunner::Context ctx;
    Expression H = ctx.define_matrix_symbolic("H", "a", "b", {"full_rank", "wide"});
    Expression y = ctx.define_matrix_symbolic("y", "a", 1);
    Expression I = ctx.define_matrix_symbolic("I", "b", "b", {"identity"});
    Expression x = ctx.define_matrix_symbolic("x", "b", 1);

    Expression math_1 = transpose(H) * inverse(H * transpose(H));
    Expression math_2 = math_1 * y + (I - math_1 * H) * x;
    auto id_1 = ctx.add(math_1);
    auto id_2 = ctx.add(math_2);
    // ctx.rewrite_and_prune({id_1, id_2});
    // ctx.lower_to_kernels();
    ctx.rewrite();
    ctx.prune_symbolic_when_kernel_available();
    auto [h_sizes, h_data] = read_matrix("examples/data/image_h.txt");
    auto [y_sizes, y_data] = read_matrix("examples/data/image_y.txt");
    auto [x_sizes, x_data] = read_matrix("examples/data/image_x.txt");

    if (h_sizes.first >= h_sizes.second) {
        std::cerr << "Size error: H must be wide (rows < cols).\n";
        return 1;
    }
    if (y_sizes.second != 1 || x_sizes.second != 1) {
        std::cerr << "Size error: y and x must be column vectors.\n";
        return 1;
    }
    if (h_sizes.first != y_sizes.first) {
        std::cerr << "Size error: H and y must have the same number of rows.\n";
        return 1;
    }
    if (h_sizes.second != x_sizes.first) {
        std::cerr << "Size error: H and x must agree on the inner dimension.\n";
        return 1;
    }

    SizeBindings concrete_sizes = {{"a", h_sizes.first}, {"b", h_sizes.second}};
    DataBindings concrete_data = {
        {"y", y_data}, {"H", h_data}, {"I", generate_identity_matrix(h_sizes.second)}, {"x", x_data}};

    auto out2 = ctx.evaluate_concrete(id_2, concrete_sizes, concrete_data);
    {
        auto out_shape = bind_shape(ctx.get_property(id_2).shape, &concrete_sizes);
        int row = std::get<int>(out_shape.first);
        int col = std::get<int>(out_shape.second);
        for (int i = 0; i < row; ++i) {
            for (int j = 0; j < col; ++j) {
                std::cout << out2[i * col + j] << " ";
            }
            std::cout << "\n";
        }
    }
    auto out1 = ctx.evaluate_concrete(id_1, concrete_sizes, concrete_data);
    {
        auto out_shape = bind_shape(ctx.get_property(id_1).shape, &concrete_sizes);
        int row = std::get<int>(out_shape.first);
        int col = std::get<int>(out_shape.second);
        for (int i = 0; i < row; ++i) {
            for (int j = 0; j < col; ++j) {
                std::cout << out1[i * col + j] << " ";
            }
            std::cout << "\n";
        }
    }
}