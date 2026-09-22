#include "MatrixIO.h"
#include "api.h"
#include "examples.h"
#include "utils.h"
#include <iostream>
#include <utility>
using namespace egraph;

int run_image() {
    auto [h_sizes, h_data] = read_matrix("data/image_h.csv");
    auto [y_sizes, y_data] = read_matrix("data/image_y.csv");
    auto [x_sizes, x_data] = read_matrix("data/image_x.csv");

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

    EGraphRunner::Context ctx;
    ctx.get_config().enable_logging = false;
    ctx.get_config().disabled_ops = {Op::Potrf_U};

    Expression y = ctx.define_matrix("y", "a", 1);
    Expression H = ctx.define_matrix("H", "a", "b", {"full_rank", "wide"});
    Expression I = ctx.define_matrix("I", "b", "b", {"identity"});
    Expression x = ctx.define_matrix("x", "b", 1);

    Expression math_1 = transpose(H) * inverse(H * transpose(H));
    Expression math_2 = math_1 * y + (I - math_1 * H) * x;

    // math_1 passed as a background expression
    ctx.optimize_symbolic(math_2, {math_1});

    DataBindings concrete_data = {
        {"y", y_data}, {"H", h_data}, {"I", generate_identity_matrix(h_sizes.second)}, {"x", x_data}};

    // run extraction for both math_1 and math_2 if math_1 is not computed explicitly when computing math_2 (does not
    // appear as a e-class in the extraction result of math_2)
    auto out2 = ctx.evaluate_concrete(concrete_sizes, concrete_data);

    auto out1 = ctx.get_preserved(math_1);
    auto out1_shape = bind_shape(ctx.get_property(math_1).shape, &concrete_sizes);
    int row1 = std::get<int>(out1_shape.first);
    int col1 = std::get<int>(out1_shape.second);
    std::string out_path1 = "data/image_result_1.csv";
    write_matrix(out_path1, row1, col1, out1);

    // Retrieve target math_2
    auto out2_shape = bind_shape(ctx.get_property().shape, &concrete_sizes);
    int row2 = std::get<int>(out2_shape.first);
    int col2 = std::get<int>(out2_shape.second);
    std::string out_path2 = "data/image_result_2.csv";
    write_matrix(out_path2, row2, col2, out2);

    return 0;
}
