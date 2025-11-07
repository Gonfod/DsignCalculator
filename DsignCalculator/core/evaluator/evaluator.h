#pragma once
#include "../tokenizer/tokenizer.h"
#include <queue>
#include <map>
#include <stack>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <string>

inline double evaluateRPNVec(const std::vector<Token>& rpn, double xValue) {
    std::stack<double> st;

    for (const Token& t : rpn) {
        if (t.type == TokenType::Number) {
            st.push(t.number);
        }
        else if (t.type == TokenType::Variable) {
            // only map variable named 'x' to xValue; other variables default to 0
            if (t.text == "x") st.push(xValue);
            else st.push(0.0);
        }
        else if (t.type == TokenType::Operator) {
            if (st.size() < 2) throw std::runtime_error("Invalid expression");
            double b = st.top(); st.pop();
            double a = st.top(); st.pop();
            if (t.text == "+") st.push(a + b);
            else if (t.text == "-") st.push(a - b);
            else if (t.text == "*") st.push(a * b);
            else if (t.text == "/") st.push(a / b);
            else if (t.text == "^") st.push(std::pow(a, b));
        }
        else if (t.type == TokenType::Function) {
            if (st.size() < (size_t)t.arity) throw std::runtime_error("Function args");

            double result = 0;
            if (t.arity == 1) {
                double a = st.top(); st.pop();
                if (t.text == "sin") result = std::sin(a);
                else if (t.text == "cos") result = std::cos(a);
                else if (t.text == "tan") result = std::tan(a);
                else if (t.text == "arcsin" || t.text == "asin") result = std::asin(a);
                else if (t.text == "arccos" || t.text == "acos") result = std::acos(a);
                else if (t.text == "arctan" || t.text == "atan") result = std::atan(a);
                else if (t.text == "sqrt") result = std::sqrt(a);
                else if (t.text == "log" || t.text == "ln") result = std::log(a);
                else if (t.text == "exp") result = std::exp(a);
                else if (t.text == "neg") result = -a;
                else if (t.text == "abs") result = std::fabs(a);
                else result = std::nan("1");
            }
            else if (t.arity == 2) {
                double b = st.top(); st.pop();
                double a = st.top(); st.pop();
                if (t.text == "pow") result = std::pow(a, b);
                else result = std::nan("1");
            }
            st.push(result);
        }
    }

    if (st.size() != 1) throw std::runtime_error("Invalid evaluation");
    return st.top();
}

// evaluate with two variables: variable named "x" and "y"
inline double evaluateRPNXY(const std::vector<Token>& rpn, double xValue, double yValue) {
    std::stack<double> st;

    for (const Token& t : rpn) {
        if (t.type == TokenType::Number) {
            st.push(t.number);
        }
        else if (t.type == TokenType::Variable) {
            if (t.text == "x") st.push(xValue);
            else if (t.text == "y") st.push(yValue);
            else /* unknown variable -> 0 */ st.push(0.0);
        }
        else if (t.type == TokenType::Operator) {
            if (st.size() < 2) throw std::runtime_error("Invalid expression");
            double b = st.top(); st.pop();
            double a = st.top(); st.pop();
            if (t.text == "+") st.push(a + b);
            else if (t.text == "-") st.push(a - b);
            else if (t.text == "*") st.push(a * b);
            else if (t.text == "/") st.push(a / b);
            else if (t.text == "^") st.push(std::pow(a, b));
        }
        else if (t.type == TokenType::Function) {
            if (st.size() < (size_t)t.arity) throw std::runtime_error("Function args");

            double result = 0;
            if (t.arity == 1) {
                double a = st.top(); st.pop();
                if (t.text == "sin") result = std::sin(a);
                else if (t.text == "cos") result = std::cos(a);
                else if (t.text == "tan") result = std::tan(a);
                else if (t.text == "arcsin" || t.text == "asin") result = std::asin(a);
                else if (t.text == "arccos" || t.text == "acos") result = std::acos(a);
                else if (t.text == "arctan" || t.text == "atan") result = std::atan(a);
                else if (t.text == "sqrt") result = std::sqrt(a);
                else if (t.text == "log" || t.text == "ln") result = std::log(a);
                else if (t.text == "exp") result = std::exp(a);
                else if (t.text == "neg") result = -a;
                else if (t.text == "abs") result = std::fabs(a);
                else result = std::nan("1");
            }
            else if (t.arity == 2) {
                double b = st.top(); st.pop();
                double a = st.top(); st.pop();
                if (t.text == "pow") result = std::pow(a, b);
                else result = std::nan("1");
            }
            st.push(result);
        }
    }

    if (st.size() != 1) throw std::runtime_error("Invalid evaluation");
    return st.top();
}

// evaluate using environment map: variable names -> values
inline double evaluateRPNEnv(const std::vector<Token>& rpn, const std::unordered_map<std::string,double>& env) {
    std::stack<double> st;
    for (const Token& t : rpn) {
        if (t.type == TokenType::Number) st.push(t.number);
        else if (t.type == TokenType::Variable) {
            auto it = env.find(t.text);
            if (it != env.end()) st.push(it->second);
            else {
                // fallback: x->0
                if (t.text == "x") st.push(0.0);
                else if (t.text == "y") st.push(0.0);
                else st.push(0.0);
            }
        }
        else if (t.type == TokenType::Operator) {
            if (st.size() < 2) throw std::runtime_error("Invalid expression");
            double b = st.top(); st.pop();
            double a = st.top(); st.pop();
            if (t.text == "+") st.push(a + b);
            else if (t.text == "-") st.push(a - b);
            else if (t.text == "*") st.push(a * b);
            else if (t.text == "/") st.push(a / b);
            else if (t.text == "^") st.push(std::pow(a, b));
        }
        else if (t.type == TokenType::Function) {
            if (st.size() < (size_t)t.arity) throw std::runtime_error("Function args");
            double result = 0;
            if (t.arity == 1) {
                double a = st.top(); st.pop();
                if (t.text == "sin") result = std::sin(a);
                else if (t.text == "cos") result = std::cos(a);
                else if (t.text == "tan") result = std::tan(a);
                else if (t.text == "arcsin" || t.text == "asin") result = std::asin(a);
                else if (t.text == "arccos" || t.text == "acos") result = std::acos(a);
                else if (t.text == "arctan" || t.text == "atan") result = std::atan(a);
                else if (t.text == "sqrt") result = std::sqrt(a);
                else if (t.text == "log" || t.text == "ln") result = std::log(a);
                else if (t.text == "exp") result = std::exp(a);
                else if (t.text == "neg") result = -a;
                else if (t.text == "abs") result = std::fabs(a);
                else result = std::nan("1");
            }
            else if (t.arity == 2) {
                double b = st.top(); st.pop();
                double a = st.top(); st.pop();
                if (t.text == "pow") result = std::pow(a, b);
                else result = std::nan("1");
            }
            st.push(result);
        }
    }
    if (st.size() != 1) throw std::runtime_error("Invalid evaluation");
    return st.top();
}
