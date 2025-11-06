#pragma once
#include <string>
#include <vector>
#include <map>

enum class TokenType {
    Number,
    Variable,
    Operator,
    Function,
    LeftParen,
    RightParen,
    Comma,
    End,
    Invalid
};

struct Token {
    TokenType type;
    std::string text;
    double number = 0.0;
    int precedence = 0;
    bool rightAssociative = false;
    int arity = 0;

    Token() : type(TokenType::Invalid) {}
    Token(TokenType t, std::string s) : type(t), text(std::move(s)) {}
};

std::vector<Token> tokenize(const std::string& expr);
