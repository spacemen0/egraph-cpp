#pragma once

#include "basic_types.h"
#include "expression.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace egraph::frontend {

enum class SymbolKind : std::uint8_t { Matrix, Scalar, ColVector, RowVector };
enum class EffortLevel : std::uint8_t { Low, Medium, High };
enum class OperationKind : std::uint8_t { ExtractBest, ExtractAll, Evaluate };

struct SourceLocation {
    uint32_t line = 1;
    uint32_t column = 1;
};

struct SymbolDecl {
    std::string name;
    SymbolKind kind = SymbolKind::Matrix;
    std::vector<std::string> property_flags;
    Shape shape{"d0", "d1"};
    SourceLocation loc;
};

struct NamedExpr {
    std::string name;
    Expression expr;
    SourceLocation loc;
};

struct OperationDirective {
    OperationKind kind = OperationKind::ExtractBest;
    std::string target_variable;
    EffortLevel effort = EffortLevel::Medium;
    std::unordered_map<std::string, std::string> data_bindings;
    SourceLocation loc;
};

struct ProgramManifest {
    std::vector<SymbolDecl> symbols;
    std::vector<NamedExpr> expressions;
    std::vector<OperationDirective> operations;
};

} // namespace egraph::frontend
