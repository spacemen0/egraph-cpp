#include "parser.h"
#include "errors.h"
#include "utils.h"
#include <cctype>
#include <memory>
#include <vector>

namespace parser {

enum class TokenType { Eof, Plus, Minus, Star, Slash, LParen, RParen, Comma, Ident, Num, Error };

struct Token {
    TokenType type;
    std::string_view text;
};

class Lexer {
    std::string_view s;
    size_t pos = 0;

    void skip_whitespace() {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
            pos++;
        }
    }

  public:
    Lexer(std::string_view s) : s(s) {}

    Token next() {
        skip_whitespace();
        if (pos >= s.size())
            return {TokenType::Eof, {}};

        char c = s[pos];
        if (c == '+') {
            pos++;
            return {TokenType::Plus, s.substr(pos - 1, 1)};
        }
        if (c == '-') {
            pos++;
            return {TokenType::Minus, s.substr(pos - 1, 1)};
        }
        if (c == '*') {
            pos++;
            return {TokenType::Star, s.substr(pos - 1, 1)};
        }
        if (c == '/') {
            pos++;
            return {TokenType::Slash, s.substr(pos - 1, 1)};
        }
        if (c == '(') {
            pos++;
            return {TokenType::LParen, s.substr(pos - 1, 1)};
        }
        if (c == ')') {
            pos++;
            return {TokenType::RParen, s.substr(pos - 1, 1)};
        }
        if (c == ',') {
            pos++;
            return {TokenType::Comma, s.substr(pos - 1, 1)};
        }

        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && pos + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[pos + 1])))) {
            size_t start = pos;
            bool has_dot = (c == '.');
            pos++;
            while (pos < s.size()) {
                if (std::isdigit(static_cast<unsigned char>(s[pos]))) {
                    pos++;
                } else if (s[pos] == '.' && !has_dot) {
                    has_dot = true;
                    pos++;
                } else {
                    break;
                }
            }
            return {TokenType::Num, s.substr(start, pos - start)};
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '?' || c == '_') {
            size_t start = pos;
            while (pos < s.size() &&
                   (std::isalnum(static_cast<unsigned char>(s[pos])) || s[pos] == '?' || s[pos] == '_')) {
                pos++;
            }
            return {TokenType::Ident, s.substr(start, pos - start)};
        }

        return {TokenType::Error, s.substr(pos++, 1)};
    }
};

namespace {
int op_precedence(Op op) {
    switch (op) {
    case Op::Add:
    case Op::Minus:
        return 10;
    case Op::Mul:
        return 20;
    default:
        return 100;
    }
}
} // namespace

struct ASTNode {
    Atom atom;
    std::vector<std::unique_ptr<ASTNode>> children;

    int precedence() const {
        if (std::holds_alternative<Op>(atom)) {
            return op_precedence(std::get<Op>(atom));
        }
        return 100;
    }

    std::string to_string() const {
        if (std::holds_alternative<Op>(atom)) {
            Op op = std::get<Op>(atom);
            if (op == Op::Add || op == Op::Mul || op == Op::Minus) {
                std::string sym;
                if (op == Op::Add)
                    sym = " + ";
                else if (op == Op::Mul)
                    sym = " * ";
                else
                    sym = " - ";

                if (children.size() == 2) {
                    int my_prec = op_precedence(op);
                    auto format_child = [&](const ASTNode &child) {
                        std::string s = child.to_string();
                        if (child.precedence() < my_prec) {
                            return "(" + s + ")";
                        }
                        return s;
                    };
                    return format_child(*children[0]) + sym + format_child(*children[1]);
                } else if (op == Op::Minus && children.size() == 1) {
                    return "-" + children[0]->to_string();
                }
            }

            std::string res = atom_to_string(atom) + "(";
            for (size_t i = 0; i < children.size(); ++i) {
                if (i > 0)
                    res += ", ";
                res += children[i]->to_string();
            }
            res += ")";
            return res;
        } else if (std::holds_alternative<uint32_t>(atom)) {
            return get_string_from_lookup(std::get<uint32_t>(atom));
        } else if (std::holds_alternative<int>(atom)) {
            return std::to_string(std::get<int>(atom));
        } else if (std::holds_alternative<ScalarExpr>(atom)) {
            return std::get<ScalarExpr>(atom).to_string();
        }
        throw std::runtime_error("Unknown atom type in ASTNode");
    }
};

class Parser {
    Lexer lexer;
    Token curr;

    void advance() {
        curr = lexer.next();
        if (curr.type == TokenType::Error) {
            throw ParseError("Unexpected character: " + std::string(curr.text));
        }
    }

    int precedence(TokenType type) {
        switch (type) {
        case TokenType::Plus:
        case TokenType::Minus:
            return 10;
        case TokenType::Star:
            return 20;
        default:
            return 0;
        }
    }

    std::unique_ptr<ASTNode> parse_prefix() {
        if (curr.type == TokenType::Num) {
            double v;
            try {
                v = std::stod(std::string(curr.text));
            } catch (...) {
                throw ParseError("Invalid number: " + std::string(curr.text));
            }
            auto node = std::make_unique<ASTNode>();
            if (v == static_cast<int>(v) && curr.text.find('.') == std::string_view::npos) {
                node->atom = static_cast<int>(v);
            } else {
                node->atom = ScalarExpr(v);
            }
            advance();
            return node;
        }

        if (curr.type == TokenType::Minus) {
            advance();
            if (curr.type == TokenType::Num) {
                double v;
                try {
                    v = std::stod(std::string(curr.text));
                } catch (...) {
                    throw ParseError("Invalid number: " + std::string(curr.text));
                }
                auto node = std::make_unique<ASTNode>();
                // index can only be positive integer
                // if (-v == static_cast<int>(-v) && curr.text.find('.') == std::string_view::npos) {
                //     node->atom = static_cast<int>(-v);
                // }

                node->atom = ScalarExpr(-v);

                advance();
                return node;
            }
            auto expr = parse_expr(30);
            auto node = std::make_unique<ASTNode>();
            node->atom = Op::Scale;

            auto neg_one = std::make_unique<ASTNode>();
            neg_one->atom = ScalarExpr(-1.0);
            node->children.push_back(std::move(expr));
            node->children.push_back(std::move(neg_one));
            return node;
        }

        if (curr.type == TokenType::LParen) {
            advance();
            auto node = parse_expr(0);
            if (curr.type != TokenType::RParen)
                throw ParseError("Expected closing parenthesis");
            advance();
            return node;
        }

        if (curr.type == TokenType::Ident) {
            std::string_view name = curr.text;
            advance();

            // Check for function call
            if (curr.type == TokenType::LParen) {
                Op op = parse_op(name);
                if (op == Op::Add || op == Op::Mul || op == Op::Minus) {
                    throw ParseError(
                        "Prefix syntax for " + std::string(name) +
                        " is no longer supported. Use infix +, *, or - instead.");
                }

                advance();
                auto node = std::make_unique<ASTNode>();
                node->atom = op;

                if (op == Op::Get) {
                    if (curr.type == TokenType::RParen)
                        throw ParseError("Get requires arguments");
                    node->children.push_back(parse_expr(0));
                    if (curr.type != TokenType::Comma)
                        throw ParseError("Expected comma in Get(Tuple, Index)");
                    advance();
                    if (curr.type == TokenType::Num) {
                        int idx = static_cast<int>(std::stod(std::string(curr.text)));
                        auto idx_node = std::make_unique<ASTNode>();
                        idx_node->atom = idx;
                        node->children.push_back(std::move(idx_node));
                        advance();
                    } else {
                        throw ParseError("Get index must be an integer literal");
                    }
                    if (curr.type != TokenType::RParen)
                        throw ParseError("Expected closing parenthesis after Get index");
                    advance();
                    return node;
                }

                if (curr.type != TokenType::RParen) {
                    while (true) {
                        node->children.push_back(parse_expr(0));
                        if (curr.type == TokenType::Comma) {
                            advance();
                        } else {
                            break;
                        }
                    }
                }
                if (curr.type != TokenType::RParen)
                    throw ParseError("Expected closing parenthesis after arguments");
                advance();
                return node;
            } else {
                auto node = std::make_unique<ASTNode>();
                node->atom = register_string_in_lookup(std::string(name));
                return node;
            }
        }

        throw ParseError("Unexpected token in expression: " + std::string(curr.text));
    }

    std::unique_ptr<ASTNode> parse_infix(std::unique_ptr<ASTNode> left, TokenType op_type) {
        advance();
        int prec = precedence(op_type);
        auto right = parse_expr(prec);

        auto node = std::make_unique<ASTNode>();
        if (op_type == TokenType::Plus)
            node->atom = Op::Add;
        else if (op_type == TokenType::Minus)
            node->atom = Op::Minus;
        else if (op_type == TokenType::Star)
            node->atom = Op::Mul;

        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        return node;
    }

    std::unique_ptr<ASTNode> parse_expr(int prec) {
        auto left = parse_prefix();
        while (curr.type != TokenType::Eof && precedence(curr.type) > prec) {
            left = parse_infix(std::move(left), curr.type);
        }
        return left;
    }

  public:
    Parser(std::string_view s) : lexer(s) { advance(); }

    std::unique_ptr<ASTNode> parse() {
        if (curr.type == TokenType::Eof)
            throw ParseError("Empty expression");
        auto node = parse_expr(0);
        if (curr.type != TokenType::Eof)
            throw ParseError("Unexpected token after expression: " + std::string(curr.text));
        return node;
    }
};

ParsedAtom parse_expression(std::string_view s) {
    Parser parser(s);
    auto ast = parser.parse();

    ParsedAtom res;
    res.atom = ast->atom;
    for (const auto &child : ast->children) {
        res.children_strings.push_back(child->to_string());
    }
    return res;
}

class ScalarParser {
    Lexer lexer;
    Token curr;

    void advance() { curr = lexer.next(); }

    ScalarExpr parse_primary() {
        if (curr.type == TokenType::Num) {
            double val = std::stod(std::string(curr.text));
            advance();
            return ScalarExpr(val);
        } else if (curr.type == TokenType::Ident) {
            char var = curr.text.empty() ? '\0' : curr.text[0];
            advance();
            return ScalarExpr(var);
        } else if (curr.type == TokenType::Minus) {
            advance();
            return {ScalarOp::Neg, {parse_primary()}};
        } else if (curr.type == TokenType::LParen) {
            advance();
            ScalarExpr expr = parse_expr();
            if (curr.type == TokenType::RParen) {
                advance();
            } else {
                throw ParseError("Expected closing parenthesis in scalar expression");
            }
            return expr;
        }
        throw ParseError("Unexpected token in scalar expression: " + std::string(curr.text));
    }

    ScalarExpr parse_term() {
        ScalarExpr left = parse_primary();
        while (curr.type == TokenType::Star || curr.type == TokenType::Slash) {
            TokenType op = curr.type;
            advance();
            ScalarExpr right = parse_primary();
            if (op == TokenType::Star) {
                left = {ScalarOp::Mul, {left, right}};
            } else {
                left = {ScalarOp::Div, {left, right}};
            }
        }
        return left;
    }

    ScalarExpr parse_expr() {
        ScalarExpr left = parse_term();
        while (curr.type == TokenType::Plus || curr.type == TokenType::Minus) {
            TokenType op = curr.type;
            advance();
            ScalarExpr right = parse_term();
            if (op == TokenType::Plus) {
                left = {ScalarOp::Add, {left, right}};
            } else {
                left = {ScalarOp::Sub, {left, right}};
            }
        }
        return left;
    }

  public:
    ScalarParser(std::string_view s) : lexer(s) { advance(); }

    ScalarExpr parse() {
        if (curr.type == TokenType::Eof)
            throw ParseError("Empty scalar expression");
        auto node = parse_expr();
        if (curr.type != TokenType::Eof)
            throw ParseError("Unexpected token after scalar expression: " + std::string(curr.text));
        return node;
    }
};

ScalarExpr parse_scalar(std::string_view s) {
    ScalarParser parser(s);
    return parser.parse();
}

} // namespace parser
