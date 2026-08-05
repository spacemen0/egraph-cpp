#include "api.h"
#include <iostream>

int main() {
    EGraphRunner::Context ctx;
    ctx.egraph = EGraph();

    Expression X = ctx.define_matrix_symbolic("X", "a", "b", {"full_rank", "tall"});
    Expression y = ctx.define_matrix_symbolic("y", "a", 1);
    Expression M = ctx.define_matrix_symbolic("M", "a", "a", {"symmetric", "positive_definite"});
    Expression target_math = inverse(transpose(X) * inverse(M) * X) * transpose(X) * inverse(M) * y;

    ctx.optimize_symbolic(target_math);

    SizeBindings concrete_sizes = {{"a", 3}, {"b", 2}};
    DataBindings concrete_data = {
        {"X", std::vector<double>{2.0, 1.0, 0.0, 1.0, 3.0, 1.0}},
        {"M",
         std::vector<double>{
             1.0,
             0.0,
             0.0,
             1.0,
             2.0,
             3.0,
             5.0,
             1.5,
             1.0,
         }},
        {"y", std::vector<double>{5.0, 10.0, 3.0}}};

    auto out1 = ctx.evaluate_concrete(concrete_sizes, concrete_data);
    for (int i = 0; i < 2; ++i) {
        std::cout << out1[i] << (i == 1 ? "" : " ");
    }
    std::cout << "\n";
}