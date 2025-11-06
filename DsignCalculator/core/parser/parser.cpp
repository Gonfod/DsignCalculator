#include "core_parser.h"
#include <stdexcept>

std::vector<Token> shuntingYard(const std::vector<Token>& tokens) {
    std::vector<Token> output;
    std::stack<Token> opstack;

    for (const auto& t : tokens) {
        switch (t.type) {
        case TokenType::Number:
        case TokenType::Variable:
            output.push_back(t);
            break;

        case TokenType::Function:
            opstack.push(t);
            break;

        case TokenType::Operator:
            while (!opstack.empty() &&
                (opstack.top().type == TokenType::Operator || opstack.top().type == TokenType::Function))
            {
                Token top = opstack.top();
                bool higher = (t.rightAssociative ? t.precedence < top.precedence : t.precedence <= top.precedence);
                if (top.type == TokenType::Function || higher) {
                    output.push_back(top);
                    opstack.pop();
                }
                else break;
            }
            opstack.push(t);
            break;

        case TokenType::Comma:
            while (!opstack.empty() && opstack.top().type != TokenType::LeftParen) {
                output.push_back(opstack.top());
                opstack.pop();
            }
            break;

        case TokenType::LeftParen:
            opstack.push(t);
            break;

        case TokenType::RightParen:
            while (!opstack.empty() && opstack.top().type != TokenType::LeftParen) {
                output.push_back(opstack.top());
                opstack.pop();
            }
            if (opstack.empty()) throw std::runtime_error("Mismatched parentheses");
            opstack.pop(); // pop '('
            if (!opstack.empty() && opstack.top().type == TokenType::Function) {
                output.push_back(opstack.top());
                opstack.pop();
            }
            break;

        case TokenType::End:
        case TokenType::Invalid:
            break;
        }
    }

    while (!opstack.empty()) {
        if (opstack.top().type == TokenType::LeftParen || opstack.top().type == TokenType::RightParen)
            throw std::runtime_error("Mismatched parentheses");
        output.push_back(opstack.top());
        opstack.pop();
    }

    return output;
}
