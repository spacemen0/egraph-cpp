#pragma once

#include "expression.h"
#include "utils.h"
#include <cctype>
#include <stack>
#include <string>
#include <vector>

// A simple shunting-yard based infix parser for Expression
namespace {

enum class TokenType {
    Number,
    Variable,
    Operator, // +, *, -, /
    Function, // inv, trans, etc
    LParen,
    RParen,
    Comma
};

struct Token {
    TokenType type;
    std::string value;
};

inline std::vector<Token> tokenize(std::string_view input) {
    std::vector<Token> tokens;
    for (size_t i = 0; i < input.size(); ++i) {
        if (std::isspace(input[i]))
            continue;

        if (std::isdigit(input[i])) {
            std::string val;
            while (i < input.size() && (std::isdigit(input[i]) || input[i] == '.')) {
                val += input[i++];
            }
            --i;
            tokens.push_back({TokenType::Number, val});
        } else if (std::isalpha(input[i]) || input[i] == '_') {
            std::string val;
            while (i < input.size() && (std::isalnum(input[i]) || input[i] == '_')) {
                val += input[i++];
            }
            // Peek next to see if it's a function
            size_t next = i;
            while (next < input.size() && std::isspace(input[next]))
                next++;
            if (next < input.size() && input[next] == '(') {
                tokens.push_back({TokenType::Function, val});
            } else {
                tokens.push_back({TokenType::Variable, val});
            }
            --i;
        } else if (input[i] == '(') {
            tokens.push_back({TokenType::LParen, "("});
        } else if (input[i] == ')') {
            tokens.push_back({TokenType::RParen, ")"});
        } else if (input[i] == ',') {
            tokens.push_back({TokenType::Comma, ","});
        } else if (input[i] == '+' || input[i] == '*' || input[i] == '-' || input[i] == '/') {
            tokens.push_back({TokenType::Operator, std::string(1, input[i])});
        } else {
            throw ParseError("Unknown character in expression: " + std::string(1, input[i]));
        }
    }
    return tokens;
}

inline int get_precedence(const std::string &op) {
    if (op == "+")
        return 1;
    if (op == "-")
        return 1;
    if (op == "*")
        return 2;
    if (op == "/")
        return 2;
    return 0;
}

inline Expression parse_infix(std::string_view input) {
    auto tokens = tokenize(input);
    std::stack<Expression> output_queue;
    std::stack<Token> operator_stack;

    auto apply_op = [&]() {
        if (operator_stack.empty())
            throw ParseError("Mismatched operators");
        auto op_token = operator_stack.top();
        operator_stack.pop();
        std::string op_name = op_token.value;

        if (output_queue.size() < 2)
            throw ParseError("Too few operands for " + op_name);

        if (op_name == "+") {
            auto rhs = std::move(output_queue.top());
            output_queue.pop();
            auto lhs = std::move(output_queue.top());
            output_queue.pop();
            std::vector<Expression> children;
            children.push_back(std::move(lhs));
            children.push_back(std::move(rhs));
            output_queue.push(Expression(Atom(Op::Add), children));
        } else if (op_name == "*") {
            auto rhs = std::move(output_queue.top());
            output_queue.pop();
            auto lhs = std::move(output_queue.top());
            output_queue.pop();
            std::vector<Expression> children;
            children.push_back(std::move(lhs));
            children.push_back(std::move(rhs));
            output_queue.push(Expression(Atom(Op::Mul), children));
        } else if (op_name == "-") {
            auto rhs = std::move(output_queue.top());
            output_queue.pop();
            auto lhs = std::move(output_queue.top());
            output_queue.pop();
            std::vector<Expression> neg_children;
            neg_children.push_back(std::move(rhs));
            Expression neg_expr(Atom(Op::Neg), neg_children);
            std::vector<Expression> add_children;
            add_children.push_back(std::move(lhs));
            add_children.push_back(std::move(neg_expr));
            output_queue.push(Expression(Atom(Op::Add), add_children));
        } else if (op_name == "/") {
            auto rhs = std::move(output_queue.top());
            output_queue.pop();
            auto lhs = std::move(output_queue.top());
            output_queue.pop();
            // a / b  => Mul(a, Inv(b))
            std::vector<Expression> inv_children;
            inv_children.push_back(std::move(rhs));
            Expression inv_expr(Atom(Op::Inv), inv_children);
            std::vector<Expression> mul_children;
            mul_children.push_back(std::move(lhs));
            mul_children.push_back(std::move(inv_expr));
            output_queue.push(Expression(Atom(Op::Mul), mul_children));
        }
    };

    auto apply_func = [&](const std::string &func_name, int arg_count) {
        if (output_queue.size() < (size_t)arg_count)
            throw ParseError("Too few arguments for " + func_name);
        std::vector<Expression> children;
        for (int i = 0; i < arg_count; ++i) {
            children.insert(children.begin(), std::move(output_queue.top()));
            output_queue.pop();
        }

        Op op;
        if (func_name == "inv")
            op = Op::Inv;
        else if (func_name == "trans")
            op = Op::Tr;
        else if (func_name == "det")
            op = Op::Det;
        else if (func_name == "log")
            op = Op::Log;
        else
            op = parse_op(func_name); // Solver, QR, etc.

        output_queue.push(Expression(Atom(op), children));
    };

    std::stack<int> arg_counts;

    for (const auto &token : tokens) {
        switch (token.type) {
        case TokenType::Number:
            output_queue.push(Expression(Atom(std::stoi(token.value)), Expression::FromAtomTag{}));
            break;
        case TokenType::Variable:
            output_queue.push(Expression(Atom(token.value), Expression::FromAtomTag{}));
            break;
        case TokenType::Function:
            operator_stack.push(token);
            arg_counts.push(1);
            break;
        case TokenType::LParen:
            operator_stack.push(token);
            break;
        case TokenType::RParen:
            while (!operator_stack.empty() && operator_stack.top().type != TokenType::LParen) {
                apply_op();
            }
            if (operator_stack.empty())
                throw ParseError("Mismatched parentheses (missing '(')");
            operator_stack.pop(); // Pop LParen
            if (!operator_stack.empty() && operator_stack.top().type == TokenType::Function) {
                apply_func(operator_stack.top().value, arg_counts.top());
                operator_stack.pop();
                arg_counts.pop();
            } else if (!arg_counts.empty()) {
                // This was a grouped expression inside a function call, e.g., Func( (A+B), C )
                // We don't increment arg_counts here because the comma or closing paren of the function will handle it.
            }
            break;
        case TokenType::Comma:
            while (!operator_stack.empty() && operator_stack.top().type != TokenType::LParen) {
                apply_op();
            }
            if (arg_counts.empty())
                throw ParseError("Comma outside of function call");
            arg_counts.top()++;
            break;
        case TokenType::Operator:
            while (!operator_stack.empty() && operator_stack.top().type == TokenType::Operator &&
                   get_precedence(operator_stack.top().value) >= get_precedence(token.value)) {
                apply_op();
            }
            operator_stack.push(token);
            break;
        }
    }

    while (!operator_stack.empty()) {
        if (operator_stack.top().type == TokenType::LParen)
            throw ParseError("Mismatched parentheses (missing ')')");
        if (operator_stack.top().type == TokenType::Function)
            throw ParseError("Mismatched parentheses (missing ')' after function " + operator_stack.top().value + ")");
        apply_op();
    }

    if (output_queue.empty()) {
        throw ParseError("Empty expression");
    }
    if (output_queue.size() > 1) {
        throw ParseError("Malformed expression (too many operands)");
    }

    return std::move(output_queue.top());
}
} // namespace
