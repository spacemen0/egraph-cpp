#include "parser.h"
#include "errors.h"
#include "utils.h"
#include <cctype>
#include <charconv>
#include <memory>
#include <vector>

namespace parser {

enum class TokenType { Eof, Plus, Minus, Star, LParen, RParen, Comma, Ident, Num, Error };

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

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '?' || c == '_') {
            size_t start = pos;
            while (pos < s.size() &&
                   (std::isalnum(static_cast<unsigned char>(s[pos])) || s[pos] == '?' || s[pos] == '_')) {
                pos++;
            }
            return {TokenType::Ident, s.substr(start, pos - start)};
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = pos;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
                pos++;
            }
            return {TokenType::Num, s.substr(start, pos - start)};
        }

        return {TokenType::Error, s.substr(pos++, 1)};
    }
};

struct ASTNode {
    Atom atom;
    std::vector<std::unique_ptr<ASTNode>> children;

    std::string to_string() const {
        if (std::holds_alternative<Op>(atom)) {
            std::string res = atom_to_string(atom) + "(";
            for (size_t i = 0; i < children.size(); ++i) {
                if (i > 0)
                    res += ", ";
                res += children[i]->to_string();
            }
            res += ")";
            return res;
        } else if (std::holds_alternative<std::string>(atom)) {
            return std::get<std::string>(atom);
        } else {
            return std::to_string(std::get<int>(atom));
        }
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
            int v;
            auto [ptr, ec] = std::from_chars(curr.text.data(), curr.text.data() + curr.text.size(), v);
            if (ec != std::errc())
                throw ParseError("Invalid integer");
            auto node = std::make_unique<ASTNode>();
            node->atom = v;
            advance();
            return node;
        }

        if (curr.type == TokenType::Minus) {
            advance();
            if (curr.type == TokenType::Num) {
                int v;
                auto [ptr, ec] = std::from_chars(curr.text.data(), curr.text.data() + curr.text.size(), v);
                if (ec != std::errc())
                    throw ParseError("Invalid integer");
                auto node = std::make_unique<ASTNode>();
                node->atom = -v;
                advance();
                return node;
            }
            auto expr = parse_expr(30);
            auto node = std::make_unique<ASTNode>();
            node->atom = Op::Minus;

            auto zero = std::make_unique<ASTNode>();
            zero->atom = 0; // Using integer 0, which might not be ideal but functional.
            node->children.push_back(std::move(zero));
            node->children.push_back(std::move(expr));
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
                advance();
                auto node = std::make_unique<ASTNode>();
                node->atom = parse_op(name);

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
                node->atom = std::string(name);
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

} // namespace parser
