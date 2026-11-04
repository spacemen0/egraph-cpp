#include "egraph_parser.h"
#include "expression.h"
#include "utils.h"
#include <any>
#include <peglib.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace egraph::frontend {

using egraph::Expression;

namespace {

const char *GRAMMAR = R"(
    Program          <- SymbolsBlock ExpressionsBlock OperationsBlock
    
    # Block 1: Symbols
    SymbolsBlock     <- 'symbols' '{' SymbolDecl* '}' ';'?
    SymbolDecl       <- Identifier ':' TypeSpec PropertyFlags? ';'
    TypeSpec         <- 'Matrix' '(' Dimension ',' Dimension ')'
                      / 'Scalar'
                      / 'ColVector' '(' Dimension ')'
                      / 'RowVector' '(' Dimension ')'
    Dimension        <- < [0-9]+ > / Identifier
    PropertyFlags    <- '{' PropertyLiteral (',' PropertyLiteral)* '}'
    PropertyLiteral  <- < 'symmetric' / 'orthogonal' / 'orthonormal' / 'identity'
                        / 'zero' / 'upper_triangular' / 'lower_triangular'
                        / 'diagonal' / 'positive_definite' / 'positive_semi_definite'
                        / 'full_rank' / 'non_singular' / 'permutation'
                        / 'tall' / 'wide' >

    # Block 2: Expressions
    ExpressionsBlock <- 'expressions' '{' ExprDecl* '}' ';'?
    ExprDecl         <- Identifier '=' Expr ';'
    Expr             <- Term (AddOp Term)*
    AddOp            <- '+' / '-'
    Term             <- Factor (MulOp Factor)*
    MulOp            <- '*' / '/'
    Factor           <- UnaryMinus / Primary
    UnaryMinus       <- '-' Factor
    Primary          <- Constant / Variable / MatrixOp / '(' Expr ')'
    Constant         <- Number
    Variable         <- Identifier
    MatrixOp         <- 'trans' '(' Expr ')' / 'inv' '(' Expr ')'

    # Block 3: Operations
    OperationsBlock  <- 'operations' '{' OperationCall* '}' ';'?
    OperationCall    <- OpKind '(' Identifier ',' DataBindings (',' EffortSpec)? ')' ';'
    OpKind           <- 'extract_best' / 'extract_all' / 'evaluate'
    DataBindings     <- '{' (DataBinding (',' DataBinding)*)? '}'
    DataBinding      <- Identifier '=' StringLiteral
    EffortSpec       <- ('effort' '=')? EffortLevel
    EffortLevel      <- 'low' / 'medium' / 'high'

    # Lexical tokens and keywords
    ReservedWord     <- ( 'symbols' / 'expressions' / 'operations'
                        / 'Matrix' / 'Scalar' / 'ColVector' / 'RowVector'
                        / 'trans' / 'inv'
                        / 'extract_best' / 'extract_all' / 'evaluate'
                        / 'effort' / 'low' / 'medium' / 'high'
                        / 'symmetric' / 'orthogonal' / 'orthonormal'
                        / 'identity' / 'zero' / 'upper_triangular'
                        / 'lower_triangular' / 'diagonal'
                        / 'positive_definite' / 'positive_semi_definite'
                        / 'full_rank' / 'non_singular' / 'permutation'
                        / 'tall' / 'wide'
                        ) ![a-zA-Z0-9_]

    Identifier       <- !ReservedWord < [a-zA-Z_][a-zA-Z0-9_]* >
    Number           <- < [0-9]+ '.' [0-9]+ / [0-9]+ > ![eE0-9a-zA-Z_]
    StringLiteral    <- < '"' [^"\r\n]* '"' >
    
    # Automatic whitespace and comment skipping
    %whitespace      <- ([ \t\r\n] / Comment)*
    %word            <- [a-zA-Z0-9_]+
    Comment          <- '//' [^\r\n]* / '#' [^\r\n]*
)";

struct ParseContext {

    // expression can either eventually represent a matrix or s scalar
    // a symbol is either a matrix or a scalar
    std::unordered_map<std::string, SymbolKind> types;
};

inline ParseContext &get_context(std::any &dt) { return *std::any_cast<ParseContext *>(dt); }

peg::parser create_parser() {
    peg::parser p(GRAMMAR);
    if (!p) {
        throw std::runtime_error("Invalid internal PEG grammar definition");
    }

    // Lexical tokens and helpers
    p["Identifier"] = [](const peg::SemanticValues &vs) -> std::string {
        return vs.token_to_string();
    };

    p["Number"] = [](const peg::SemanticValues &vs) -> double {
        return std::stod(vs.token_to_string());
    };

    p["StringLiteral"] = [](const peg::SemanticValues &vs) -> std::string {
        auto tok = vs.token();
        return std::string(tok.substr(1, tok.size() - 2));
    };

    p["Dimension"] = [](const peg::SemanticValues &vs) -> Size {
        if (vs.choice() == 0) {
            return {std::stoi(vs.token_to_string())};
        }
        return {std::any_cast<std::string>(vs[0])};
    };

    p["TypeSpec"] = [](const peg::SemanticValues &vs) -> std::pair<SymbolKind, Shape> {
        switch (vs.choice()) {
        case 0:
            return {SymbolKind::Matrix, {std::any_cast<Size>(vs[0]), std::any_cast<Size>(vs[1])}};
        case 1:
            return {SymbolKind::Scalar, {Size(1), Size(1)}};
        case 2:
            return {SymbolKind::ColVector, {std::any_cast<Size>(vs[0]), Size(1)}};
        case 3:
            return {SymbolKind::RowVector, {Size(1), std::any_cast<Size>(vs[0])}};
        default:
            throw std::runtime_error("Unknown type");
        }
    };
    p["PropertyLiteral"] = [](const peg::SemanticValues &vs) -> std::string {
        return vs.token_to_string();
    };

    p["EffortLevel"] = [](const peg::SemanticValues &vs) -> EffortLevel {
        return static_cast<EffortLevel>(vs.choice());
    };

    p["AddOp"] = [](const peg::SemanticValues &vs) -> char {
        return vs.token()[0];
    };

    p["MulOp"] = [](const peg::SemanticValues &vs) -> char {
        return vs.token()[0];
    };

    p["OpKind"] = [](const peg::SemanticValues &vs) -> OperationKind {
        return static_cast<OperationKind>(vs.choice());
    };

    // Expressions
    p["Constant"] = [](const peg::SemanticValues &vs) -> Expression {
        return Expression(std::any_cast<double>(vs[0]));
    };

    p["Variable"] = [](const peg::SemanticValues &vs, std::any &dt) -> Expression {
        auto name = std::any_cast<std::string>(vs[0]);
        const auto &ctx = get_context(dt);
        auto it = ctx.types.find(name);
        // if not defined in symbols
        if (it == ctx.types.end()) {
            auto [line, col] = vs.line_info();
            throw CompileError(
                "Undefined identifier '" + name + "'",
                SourceLocation{static_cast<uint32_t>(line), static_cast<uint32_t>(col)});
        }
        if (it->second == SymbolKind::Scalar) {
            return Expression(ScalarExpr(name));
        }
        return Expression(Atom(register_string_in_lookup(name)), {});
    };

    p["UnaryMinus"] = [](const peg::SemanticValues &vs) -> Expression {
        auto inner = std::any_cast<Expression>(vs[0]);
        if (const auto *s = std::get_if<ScalarExpr>(&inner.atom)) {
            return (s->op == ScalarOp::Value) ? Expression(-s->val) : Expression(ScalarExpr(ScalarOp::Neg, {*s}));
        }
        return -inner;
    };

    p["MatrixOp"] = [](const peg::SemanticValues &vs) -> Expression {
        auto inner = std::any_cast<Expression>(vs[0]);
        if (std::holds_alternative<ScalarExpr>(inner.atom)) {
            auto [line, col] = vs.line_info();
            throw CompileError(
                vs.choice() == 0 ? "Cannot apply 'trans' to a scalar"
                                 : "Cannot apply 'inv' to a scalar; express via 1.0 / scalar",
                SourceLocation{static_cast<uint32_t>(line), static_cast<uint32_t>(col)});
        }
        return (vs.choice() == 0) ? transpose(inner) : inverse(inner);
    };

    p["Term"] = [](const peg::SemanticValues &vs) -> Expression {
        auto acc = std::any_cast<Expression>(vs[0]);
        for (size_t i = 1; i < vs.size(); i += 2) {
            char op = std::any_cast<char>(vs[i]);
            auto rhs = std::any_cast<Expression>(vs[i + 1]);
            const auto *acc_s = std::get_if<ScalarExpr>(&acc.atom);
            const auto *rhs_s = std::get_if<ScalarExpr>(&rhs.atom);

            if (op == '*') {
                if (acc_s && rhs_s) {
                    acc = (acc_s->op == ScalarOp::Value && rhs_s->op == ScalarOp::Value)
                              ? Expression(acc_s->val * rhs_s->val)
                              : Expression(*acc_s * *rhs_s);
                } else if (acc_s) {
                    acc = scale(rhs, *acc_s);
                } else if (rhs_s) {
                    acc = scale(acc, *rhs_s);
                } else {
                    acc = acc * rhs;
                }
            } else {
                auto [line, col] = vs.line_info();
                SourceLocation loc{static_cast<uint32_t>(line), static_cast<uint32_t>(col)};
                if (!rhs_s) {
                    throw CompileError("Direct matrix division is unsupported; express via inv", loc);
                }
                if (rhs_s->op == ScalarOp::Value && rhs_s->val == 0.0) {
                    throw CompileError("Division by zero", loc);
                }
                if (acc_s) {
                    acc = (acc_s->op == ScalarOp::Value && rhs_s->op == ScalarOp::Value)
                              ? Expression(acc_s->val / rhs_s->val)
                              : Expression(*acc_s / *rhs_s);
                } else { // Matrix / scalar
                    acc = (rhs_s->op == ScalarOp::Value) ? scale(acc, 1.0 / rhs_s->val)
                                                         : scale(acc, ScalarExpr(1.0) / *rhs_s);
                }
            }
        }
        return acc;
    };

    p["Expr"] = [](const peg::SemanticValues &vs) -> Expression {
        auto acc = std::any_cast<Expression>(vs[0]);
        for (size_t i = 1; i < vs.size(); i += 2) {
            char op = std::any_cast<char>(vs[i]);
            auto rhs = std::any_cast<Expression>(vs[i + 1]);
            const auto *acc_s = std::get_if<ScalarExpr>(&acc.atom);
            const auto *rhs_s = std::get_if<ScalarExpr>(&rhs.atom);

            if ((acc_s != nullptr) != (rhs_s != nullptr)) {
                auto [line, col] = vs.line_info();
                throw CompileError(
                    "Cannot add or subtract a scalar and a matrix directly",
                    SourceLocation{static_cast<uint32_t>(line), static_cast<uint32_t>(col)});
            }

            if (acc_s) {
                acc = (acc_s->op == ScalarOp::Value && rhs_s->op == ScalarOp::Value)
                          ? Expression(op == '+' ? acc_s->val + rhs_s->val : acc_s->val - rhs_s->val)
                          : Expression(op == '+' ? *acc_s + *rhs_s : *acc_s - *rhs_s);
            } else {
                // Both are matrices
                acc = (op == '+') ? (acc + rhs) : (acc - rhs);
            }
        }
        return acc;
    };

    p["ExprDecl"] = [](const peg::SemanticValues &vs, std::any &dt) -> NamedExpr {
        auto [line, col] = vs.line_info();
        NamedExpr decl{
            std::any_cast<std::string>(vs[0]), std::any_cast<Expression>(vs[1]),
            SourceLocation{static_cast<uint32_t>(line), static_cast<uint32_t>(col)}};
        auto &ctx = get_context(dt);
        if (ctx.types.contains(decl.name)) {
            throw CompileError("Duplicate identifier definition: " + decl.name, decl.loc);
        }
        ctx.types[decl.name] =
            std::holds_alternative<ScalarExpr>(decl.expr.atom) ? SymbolKind::Scalar : SymbolKind::Matrix;
        return decl;
    };

    p["ExpressionsBlock"] = [](const peg::SemanticValues &vs) -> std::vector<NamedExpr> {
        std::vector<NamedExpr> exprs;
        exprs.reserve(vs.size());
        for (const auto &v : vs) {
            exprs.push_back(std::any_cast<NamedExpr>(v));
        }
        return exprs;
    };

    // Symbols
    p["PropertyFlags"] = [](const peg::SemanticValues &vs) -> std::vector<std::string> {
        std::vector<std::string> props;
        props.reserve(vs.size());
        for (const auto &v : vs) {
            props.push_back(std::any_cast<std::string>(v));
        }
        return props;
    };

    p["SymbolDecl"] = [](const peg::SemanticValues &vs, std::any &dt) -> SymbolDecl {
        auto [line, col] = vs.line_info();
        SymbolDecl sym;
        sym.name = std::any_cast<std::string>(vs[0]);
        auto [kind, shape] = std::any_cast<std::pair<SymbolKind, Shape>>(vs[1]);
        sym.kind = kind;
        sym.shape = shape;
        sym.loc = SourceLocation{static_cast<uint32_t>(line), static_cast<uint32_t>(col)};
        if (vs.size() > 2) {
            sym.property_flags = std::any_cast<std::vector<std::string>>(vs[2]);
        }

        auto &ctx = get_context(dt);
        if (ctx.types.contains(sym.name)) {
            throw CompileError("Duplicate symbol declaration: " + sym.name, sym.loc);
        }
        ctx.types[sym.name] = sym.kind;
        return sym;
    };

    p["SymbolsBlock"] = [](const peg::SemanticValues &vs) -> std::vector<SymbolDecl> {
        std::vector<SymbolDecl> symbols;
        symbols.reserve(vs.size());
        for (const auto &v : vs) {
            symbols.push_back(std::any_cast<SymbolDecl>(v));
        }
        return symbols;
    };

    // Operations
    p["DataBinding"] = [](const peg::SemanticValues &vs) -> std::pair<std::string, std::string> {
        return {std::any_cast<std::string>(vs[0]), std::any_cast<std::string>(vs[1])};
    };

    p["DataBindings"] = [](const peg::SemanticValues &vs) -> std::unordered_map<std::string, std::string> {
        std::unordered_map<std::string, std::string> map;
        map.reserve(vs.size());
        for (const auto &v : vs) {
            auto pair = std::any_cast<std::pair<std::string, std::string>>(v);
            if (!map.emplace(pair.first, pair.second).second) {
                auto [line, col] = vs.line_info();
                throw CompileError(
                    "Duplicate data binding for '" + pair.first + "'",
                    SourceLocation{static_cast<uint32_t>(line), static_cast<uint32_t>(col)});
            }
        }
        return map;
    };

    p["EffortSpec"] = [](const peg::SemanticValues &vs) -> EffortLevel {
        return std::any_cast<EffortLevel>(vs[0]);
    };

    p["OperationCall"] = [](const peg::SemanticValues &vs, std::any &dt) -> OperationDesc {
        OperationDesc op;
        auto [line, col] = vs.line_info();
        op.loc = SourceLocation{static_cast<uint32_t>(line), static_cast<uint32_t>(col)};
        op.kind = std::any_cast<OperationKind>(vs[0]);
        op.target_variable = std::any_cast<std::string>(vs[1]);
        if (!get_context(dt).types.contains(op.target_variable)) {
            throw CompileError("Undefined operation target '" + op.target_variable + "'", op.loc);
        }
        op.data_bindings = std::any_cast<std::unordered_map<std::string, std::string>>(vs[2]);
        if (vs.size() > 3) {
            op.effort = std::any_cast<EffortLevel>(vs[3]);
        }
        if (op.data_bindings.empty()) {
            throw CompileError("Missing required data_bindings for operation directive", op.loc);
        }
        return op;
    };

    p["OperationsBlock"] = [](const peg::SemanticValues &vs) -> std::vector<OperationDesc> {
        std::vector<OperationDesc> ops;
        ops.reserve(vs.size());
        for (const auto &v : vs) {
            ops.push_back(std::any_cast<OperationDesc>(v));
        }
        return ops;
    };

    p["Program"] = [](const peg::SemanticValues &vs) -> ProgramManifest {
        return {
            std::any_cast<std::vector<SymbolDecl>>(vs[0]),
            std::any_cast<std::vector<NamedExpr>>(vs[1]),
            std::any_cast<std::vector<OperationDesc>>(vs[2]),
        };
    };

    return p;
}

Expression substitute_identifiers(
    const Expression &expr, const std::unordered_map<std::string, Expression> &expr_map,
    std::unordered_set<std::string> &active_stack, const std::unordered_map<std::string, SourceLocation> &expr_locs,
    SourceLocation loc);

ScalarExpr substitute_scalar(
    const ScalarExpr &s, const std::unordered_map<std::string, Expression> &expr_map,
    std::unordered_set<std::string> &active_stack, const std::unordered_map<std::string, SourceLocation> &expr_locs,
    SourceLocation loc) {
    if (s.op == ScalarOp::Var) {
        auto it = expr_map.find(s.var);
        if (it != expr_map.end()) {
            if (active_stack.contains(s.var)) {
                throw CompileError(
                    "Cyclic dependency detected in expression definitions for '" + s.var + "'", expr_locs.at(s.var));
            }
            active_stack.insert(s.var);
            Expression inlined = substitute_identifiers(it->second, expr_map, active_stack, expr_locs, loc);
            active_stack.erase(s.var);
            if (const auto *inlined_s = std::get_if<ScalarExpr>(&inlined.atom)) {
                return *inlined_s;
            }
            throw CompileError("Referenced expression '" + s.var + "' is not a scalar expression", expr_locs.at(s.var));
        }
        return s;
    }
    ScalarExpr res = s;
    bool all_constant = true;
    for (auto &child : res.children) {
        child = substitute_scalar(child, expr_map, active_stack, expr_locs, loc);
        all_constant = all_constant && child.op == ScalarOp::Value;
    }
    // Fold as substitution returns, including constant denominators with symbolic numerators.
    if (res.op == ScalarOp::Div && res.children[1].op == ScalarOp::Value && res.children[1].val == 0.0) {
        throw CompileError("Division by zero", loc);
    }
    if (all_constant) {
        return {res.evaluate()};
    }
    return res;
}

Expression substitute_identifiers(
    const Expression &expr, const std::unordered_map<std::string, Expression> &expr_map,
    std::unordered_set<std::string> &active_stack, const std::unordered_map<std::string, SourceLocation> &expr_locs,
    SourceLocation loc) {

    if (const auto *sym = std::get_if<uint32_t>(&expr.atom)) {
        std::string name = get_string_from_lookup(*sym);
        auto it = expr_map.find(name);
        if (it != expr_map.end()) {
            if (active_stack.contains(name)) {
                throw CompileError(
                    "Cyclic dependency detected in expression definitions for '" + name + "'", expr_locs.at(name));
            }
            active_stack.insert(name);
            Expression inlined = substitute_identifiers(it->second, expr_map, active_stack, expr_locs, loc);
            active_stack.erase(name);
            return inlined;
        }
        return expr;
    }

    if (const auto *scalar = std::get_if<ScalarExpr>(&expr.atom)) {
        return Expression(substitute_scalar(*scalar, expr_map, active_stack, expr_locs, loc));
    }

    Expression res = expr;
    for (size_t i = 0; i < expr.children.size(); ++i) {
        res.children[i] = substitute_identifiers(expr.children[i], expr_map, active_stack, expr_locs, loc);
    }
    return res;
}

void inline_intermediate_expressions(ProgramManifest &prog) {
    std::unordered_map<std::string, Expression> raw_map;
    std::unordered_map<std::string, SourceLocation> loc_map;
    for (const auto &ed : prog.expressions) {
        raw_map[ed.name] = ed.expr;
        loc_map[ed.name] = ed.loc;
    }

    for (auto &ed : prog.expressions) {
        std::unordered_set<std::string> active_stack;
        active_stack.insert(ed.name);
        ed.expr = substitute_identifiers(ed.expr, raw_map, active_stack, loc_map, ed.loc);
    }
}
} // namespace

ProgramManifest parse_program(std::string_view source) {
    peg::parser parser = create_parser();

    SourceLocation err_loc{1, 1};
    std::string err_msg = "Syntax error";
    parser.set_logger([&](size_t line, size_t col, const std::string &msg, const std::string &) {
        err_loc = SourceLocation{static_cast<uint32_t>(line), static_cast<uint32_t>(col)};
        err_msg = msg;
    });

    ParseContext ctx;
    std::any dt = &ctx;
    ProgramManifest prog;
    if (!parser.parse(source, dt, prog)) {
        throw SyntaxError(err_msg, err_loc);
    }

    inline_intermediate_expressions(prog);
    return prog;
}

} // namespace egraph::frontend
