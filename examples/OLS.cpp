#include "api.h"

int main() {
    EGraphRunner::Context ctx;
    ctx.egraph = EGraph();

    ctx.define_matrix_symbolic("M", "A", "B", {"full_rank", "tall"});
    ctx.define_matrix_symbolic("n", "A", 1);
    Expression M("M");
    Expression n("n");
    Expression target_math = (inverse(transpose(M) * M) * transpose(M)) * n;

    Id target_id = ctx.optimize_symbolic(target_math, {"A", "B"});

    SizeBindings concrete_sizes = {{"A", 3}, {"B", 2}};
    DataBindings concrete_data = {
        {"M", std::vector<double>{2.0, 1.0, 0.0, 1.0, 3.0, 1.0}}, {"n", std::vector<double>{5.0, 10.0, 3.0}}};

    auto out1 = ctx.evaluate_concrete(target_id, concrete_sizes, concrete_data);
    for (int i = 0; i < 2; ++i) {
        std::cout << out1[i] << (i == 1 ? "" : " ");
    }
    std::cout << "\n";
}