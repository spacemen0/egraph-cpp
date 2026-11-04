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

    // Initialize the egraph context and parse the symbols
    try {
        auto program = egraph::frontend::parse_program(content);
        egraph::EGraphRunner::Context ctx;
        for (const auto &symbol : program.symbols) {
            if (symbol.kind == egraph::frontend::SymbolKind::Matrix ||
                symbol.kind == egraph::frontend::SymbolKind::ColVector ||
                symbol.kind == egraph::frontend::SymbolKind::RowVector) {
                std::visit(
                    [&](const auto &rows, const auto &cols) {
                        ctx.define_matrix(symbol.name, rows, cols, symbol.property_flags);
                    },
                    symbol.shape.first, symbol.shape.second);
            } else if (symbol.kind == egraph::frontend::SymbolKind::Scalar) {
                std::cout << "[INFO] Defined scalar symbol (but no need to do anything here in runner): " << symbol.name
                          << "\n";
            }
        }

        // Add Expressions (here we can remove the substituion pass and use a name to expression mapping instead)
        for (const auto &named_expr : program.expressions) {
            ctx.add(named_expr.expr);
        }
        ctx.get_egraph().to_img("./egraph", "svg");
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}