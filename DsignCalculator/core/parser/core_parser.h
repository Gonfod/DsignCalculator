#pragma once
#include "../tokenizer/tokenizer.h"
#include <vector>
#include <stack>

std::vector<Token> shuntingYard(const std::vector<Token>& tokens);
