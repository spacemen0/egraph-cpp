#include "api.h"
#include "basic_types.h"
#include "egraph_parser.h"
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <script_file>\n";
        return 1;
    }
    auto filename = std::string(argv[1]);
    std::ifstream input(filename);
    if (!input.is_open()) {
        std::cerr << "Failed to open script file: " << filename << "\n";
        return 1;
    }

    // convert input to string:
    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    std::unordered_map<std::string, egraph::Id> expr_to_id;
    // Initialize the egraph context and parse the symbols
    try {
        auto program = egraph::frontend::parse_program(content);
        egraph::EGraphRunner::Context ctx;
        for (const auto &symbol : program.symbols) {
            if (symbol.kind == egraph::frontend::SymbolKind::Matrix ||
                symbol.kind == egraph::frontend::SymbolKind::ColVector ||
                symbol.kind == egraph::frontend::SymbolKind::RowVector) {
                std::visit([&](const auto &rows, const auto &cols) {
                    ctx.define_matrix(symbol.name, rows, cols, symbol.property_flags);
                }, symbol.shape.first, symbol.shape.second);
            } else if (symbol.kind == egraph::frontend::SymbolKind::Scalar) {
                std::cout << "[INFO] Defined scalar symbol (but no need to do anything here in runner): " << symbol.name
                          << "\n";
            }
        }

        // Add Expressions (here we can remove the substituion pass and use a name to expression mapping instead)
        for (const auto &named_expr : program.expressions) {
            expr_to_id[named_expr.name] = ctx.add(named_expr.expr);
        }
        ctx.get_egraph().to_img("./egraph", "svg");
        for (auto &op : program.operations) {
            std::cout << "[INFO] Operation: " << op.target_variable << " with kind: " << static_cast<int>(op.kind)
                      << "\n";
            switch (op.kind) {
            case egraph::frontend::OperationKind::ExtractBest: {
                // ctx.optimize_symbolic();
                // auto result = ctx.extract(expr_to_id[op.target_variable], op.data_bindings);
                // std::cout << "[RESULT] Extracted best expression for " << op.target_variable << ": "
                //           << result.expr.to_string() << "\n";
                // break;
                break;
            }
            case egraph::frontend::OperationKind::ExtractAll: {
                // ctx.optimize_symbolic();
                // auto results = ctx.extract(expr_to_id[op.target_variable], op.data_bindings);
                // std::cout << "[RESULT] Extracted all expressions for " << op.target_variable << ":\n";
                // for (const auto &res : results) {
                //     std::cout << "  - " << res.expr.to_string() << "\n";
                // }
                break;
            }
            case egraph::frontend::OperationKind::Evaluate: {
                // ctx.optimize_symbolic();
                // auto result = ctx.extract(expr_to_id[op.target_variable], op.data_bindings);
                // std::cout << "[RESULT] Evaluated expression for " << op.target_variable << ":\n";
                // for (const auto &val : result.values) {
                //     std::cout << "  - " << val << "\n";
                // }
                break;
            }
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}