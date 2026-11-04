#include "api.h"
#include "basic_types.h"
#include "egraph_parser.h"
#include <fstream>
#include <iostream>

// NOLINTNEXTLINE(bugprone-exception-escape): MSVC std::visit is reported despite the catch-all below.
int main(int argc, char **argv) {
    try {
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
        std::unordered_map<std::string, egraph::Id> outputs;
        // Initialize the egraph context and parse the symbols
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

        // Definitions alone do not request outputs. Register all operation targets
        // before optimizing so later operations' roots survive pruning as well.
        std::unordered_map<std::string, egraph::Expression> expressions;
        for (const auto &named_expr : program.expressions) {
            expressions.emplace(named_expr.name, named_expr.expr);
        }
        for (const auto &op : program.operations) {
            if (!outputs.contains(op.target_variable)) {
                auto it = expressions.find(op.target_variable);
                auto expr = it != expressions.end() ? it->second : egraph::Expression(op.target_variable);
                outputs.emplace(op.target_variable, ctx.add_output(expr));
            }
        }
        ctx.get_egraph().to_img("./egraph", "svg");
        for (auto &op : program.operations) {
            std::cout << "[INFO] Operation: " << op.target_variable << " with kind: " << static_cast<int>(op.kind)
                      << "\n";
            switch (op.kind) {
            case egraph::frontend::OperationKind::ExtractBest:
            case egraph::frontend::OperationKind::ExtractAll:
            case egraph::frontend::OperationKind::Evaluate:
                // Operation execution is not implemented in this runner yet.
                break;
            }
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Error: unknown exception\n";
        return 1;
    }
}
