#include "api.h"
#include <iostream>
#include <vector>

int main() {
    EGraphRunner::Context ctx;
    ctx.enable_logging();
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
    SizeBindings concrete_sizes = {{"a", 1000}, {"b", 5000}};
    DataBindings concrete_data = {
        {"y", generate_random_vector(1000)},
        {"x", generate_random_vector(5000)},
        {"H", generate_random_vector(1000 * 5000)},
        {"I", generate_identity_matrix(5000)}};

    auto out2 = ctx.evaluate_concrete(id_2, concrete_sizes, concrete_data);
    std::cout << "Evaluated out2, first 5 elements: \n";
    for (int i = 0; i < std::min((int)out2.size(), 5); ++i) {
        std::cout << out2[i] << " ";
    }
    std::cout << "\n";
    auto out1 = ctx.evaluate_concrete(id_1, concrete_sizes, concrete_data);
    // ideally this result can be read directly from the
    // intermediate results from out2 evaluation
    std::cout << "Evaluated out1, first 5 elements: \n";
    for (int i = 0; i < std::min((int)out1.size(), 5); ++i) {
        std::cout << out1[i] << " ";
    }
    std::cout << "\n";
}