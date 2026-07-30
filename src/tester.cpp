#include "api.h"
#include <iostream>
#include <vector>

int main() {
    Egraph::Context ctx;
    PropertyTable pt;
    ctx.egraph = EGraph(pt);

    ctx.define_matrix_symbolic("M", "A", "B", {"full_rank", "tall"});
    ctx.define_matrix_symbolic("n", "A", 1);
    Expression M("M");
    Expression n("n");

    Expression target_math = (inverse(transpose(M) * M) * transpose(M)) * n;
    Id target_id = ctx.optimize_symbolic(target_math, {"A", "B"});

    int A, B;
    while (std::cin >> A >> B) {
        std::vector<double> M_data(A * B);
        for (int i = 0; i < A * B; ++i) {
            std::cin >> M_data[i];
        }
        std::vector<double> n_data(A);
        for (int i = 0; i < A; ++i) {
            std::cin >> n_data[i];
        }

        SizeBindings concrete_sizes = {{"A", A}, {"B", B}};
        DataBindings concrete_data = {{"M", M_data}, {"n", n_data}};

        auto out1 = ctx.evaluate_concrete(target_id, concrete_sizes, concrete_data);

        for (int i = 0; i < B; ++i) {
            std::cout << out1[i] << (i == B - 1 ? "" : " ");
        }
        std::cout << "\n";
    }
    return 0;
}