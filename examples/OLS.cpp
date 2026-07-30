#include "api.h"
#include <iostream>

int main() {
    EGraphRunner::Context ctx;
    ctx.egraph = EGraph();

    Expression M = ctx.define_matrix_symbolic("M", "a", "b", {"full_rank", "tall"});
    Expression n = ctx.define_matrix_symbolic("n", "a", 1);
    Expression target_math = (inverse(transpose(M) * M) * transpose(M)) * n;

    ctx.optimize_symbolic(target_math, {"a", "b"});

    SizeBindings concrete_sizes = {{"a", 3}, {"b", 2}};
    DataBindings concrete_data = {
        {"M", std::vector<double>{2.0, 1.0, 0.0, 1.0, 3.0, 1.0}}, {"n", std::vector<double>{5.0, 10.0, 3.0}}};

    auto out1 = ctx.evaluate_concrete(concrete_sizes, concrete_data);
    for (int i = 0; i < 2; ++i) {
        std::cout << out1[i] << (i == 1 ? "" : " ");
    }
    std::cout << "\n";
}