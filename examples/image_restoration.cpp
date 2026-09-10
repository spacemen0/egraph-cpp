#include "MatrixIO.h"
#include "api.h"
#include "examples.h"
#include "utils.h"
#include <iostream>
#include <utility>
using namespace egraph;

int run_image() {
    auto [h_sizes, h_data] = read_matrix("examples/data/image_h.csv");
    auto [y_sizes, y_data] = read_matrix("examples/data/image_y.csv");
    auto [x_sizes, x_data] = read_matrix("examples/data/image_x.csv");

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

    // Context 1: Optimize and evaluate math_1 (pseudo-inverse of wide H)
    {
        EGraphRunner::Context ctx1;
        ctx1.get_config().enable_logging = false;
        Expression H1 = ctx1.define_matrix_symbolic("H", "a", "b", {"full_rank", "wide"});
        Expression math_1 = transpose(H1) * inverse(H1 * transpose(H1));

        ctx1.optimize_symbolic(math_1);

        DataBindings concrete_data1 = {{"H", h_data}};
        auto out1 = ctx1.evaluate_concrete(concrete_sizes, concrete_data1);
        auto out1_shape = bind_shape(ctx1.get_property().shape, &concrete_sizes);
        int row1 = std::get<int>(out1_shape.first);
        int col1 = std::get<int>(out1_shape.second);
        std::string out_path1 = "examples/data/image_result_1.csv";
        write_matrix(out_path1, row1, col1, out1);
    }

    // Context 2: Optimize and evaluate math_2 (image restoration formula)
    {
        EGraphRunner::Context ctx2;
        ctx2.get_config().enable_logging = false;
        Expression y = ctx2.define_matrix_symbolic("y", "a", 1);
        Expression H2 = ctx2.define_matrix_symbolic("H", "a", "b", {"full_rank", "wide"});
        Expression I = ctx2.define_matrix_symbolic("I", "b", "b", {"identity"});
        Expression x = ctx2.define_matrix_symbolic("x", "b", 1);

        Expression math_1_in_2 = transpose(H2) * inverse(H2 * transpose(H2));
        Expression math_2 = math_1_in_2 * y + (I - math_1_in_2 * H2) * x;

        ctx2.optimize_symbolic(math_2);

        DataBindings concrete_data2 = {
            {"y", y_data}, {"H", h_data}, {"I", generate_identity_matrix(h_sizes.second)}, {"x", x_data}};
        auto out2 = ctx2.evaluate_concrete(concrete_sizes, concrete_data2);
        auto out2_shape = bind_shape(ctx2.get_property().shape, &concrete_sizes);
        int row2 = std::get<int>(out2_shape.first);
        int col2 = std::get<int>(out2_shape.second);
        std::string out_path2 = "examples/data/image_result_2.csv";
        write_matrix(out_path2, row2, col2, out2);
    }

    return 0;
}
