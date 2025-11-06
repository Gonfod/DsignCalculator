#include "tokenizer.h"
#include <cctype>
#include <cmath>
#include <stdexcept>

static bool isOperatorChar(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^';
}

static const std::map<std::string, std::pair<int, bool>> operator_table = {
    {"+",{2,false}}, {"-",{2,false}},
    {"*",{3,false}}, {"/",{3,false}},
    {"^",{4,true}}
};

static const std::map<std::string, int> function_table = {
    {"sin",1},{"cos",1},{"tan",1},{"asin",1},{"acos",1},{"atan",1},{"arcsin",1},{"arccos",1},{"arctan",1},{"sqrt",1},{"log",1},{"ln",1},
    {"exp",1},{"pow",2},{"neg",1}
};

static const std::map<std::string, double> constant_table = {
    {"pi", 3.14159265358979323846},
    {"e",  2.71828182845904523536},
    {"phi", (1.0 + std::sqrt(5.0)) / 2.0}
};

std::vector<Token> tokenize(const std::string& expr) {
    std::vector<Token> tokens;
    size_t i = 0;

    auto push_token = [&](const Token& t) {
        // insert implicit multiplication if previous token is Number, Variable, or RightParen
        // and current token is Variable, Function, Number, or LeftParen
        if (!tokens.empty()) {
            TokenType prev = tokens.back().type;
            TokenType cur = t.type;
            bool prev_is_value = (prev == TokenType::Number || prev == TokenType::Variable || prev == TokenType::RightParen);
            bool cur_is_value = (cur == TokenType::Variable || cur == TokenType::Function || cur == TokenType::LeftParen || cur == TokenType::Number);
            if (prev_is_value && cur_is_value) {
                Token mul(TokenType::Operator, "*");
                mul.precedence = operator_table.at("*").first;
                mul.rightAssociative = operator_table.at("*").second;
                tokens.push_back(mul);
            }
        }
        tokens.push_back(t);
    };

    while (i < expr.size()) {
        char c = expr[i];
        if (isspace((unsigned char)c)) { ++i; continue; }

        if (isdigit((unsigned char)c) || c == '.') {
            size_t start = i;
            bool dot_seen = false;
            while (i < expr.size() && (isdigit((unsigned char)expr[i]) || expr[i] == '.')) {
                if (expr[i] == '.') {
                    if (dot_seen) break;
                    dot_seen = true;
                }
                ++i;
            }
            double val = std::stod(expr.substr(start, i - start));
            Token t(TokenType::Number, expr.substr(start, i - start));
            t.number = val;
            push_token(t);
            continue;
        }

        if (isalpha((unsigned char)c)) {
            size_t start = i;
            while (i < expr.size() && isalpha((unsigned char)expr[i])) ++i;
            std::string name = expr.substr(start, i - start);

            if (function_table.count(name)) {
                Token t(TokenType::Function, name);
                t.arity = function_table.at(name);
                push_token(t);
            }
            else if (constant_table.count(name)) {
                Token t(TokenType::Number, name);
                t.number = constant_table.at(name);
                push_token(t);
            }
            else {
                Token t(TokenType::Variable, name);
                push_token(t);
            }
            continue;
        }

        if (c == '-') {
            if (tokens.empty() ||
                tokens.back().type == TokenType::Operator ||
                tokens.back().type == TokenType::LeftParen ||
                tokens.back().type == TokenType::Comma)
            {
                Token t(TokenType::Function, "neg");
                t.arity = 1;
                push_token(t);
                ++i;
                continue;
            }
        }

        if (isOperatorChar(c)) {
            std::string op(1, c);
            Token t(TokenType::Operator, op);
            t.precedence = operator_table.at(op).first;
            t.rightAssociative = operator_table.at(op).second;
            tokens.push_back(t);
            ++i;
            continue;
        }

        if (c == '(') { push_token(Token(TokenType::LeftParen, "(")); ++i; continue; }
        if (c == ')') { tokens.push_back(Token(TokenType::RightParen, ")")); ++i; continue; }
        if (c == ',') { tokens.push_back(Token(TokenType::Comma, ",")); ++i; continue; }

        tokens.push_back(Token(TokenType::Invalid, std::string(1, c)));
        ++i;
    }

    tokens.push_back(Token(TokenType::End, ""));
    return tokens;
}
