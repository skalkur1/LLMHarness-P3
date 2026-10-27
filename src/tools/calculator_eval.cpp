// src/tools/calculator_eval.cpp
// calc_evaluate: the two-stack algorithm ;


#include "tools/calculator.h"

namespace {

using Kind = CalcToken::Kind;

int precedence(Kind op) {
    if (op == Kind::Times || op == Kind::Divide) return 2;
    if (op == Kind::Plus || op == Kind::Minus) return 1;
    return 0;
}

// Pops one operator and two values, pushes `left op right`.
bool apply_top(std::vector<double>& values, std::vector<Kind>& ops, std::string* error) {
    if (ops.empty() || values.size() < 2) {
        *error = "malformed expression";
        return false;
    }

    Kind op = ops.back();
    ops.pop_back();
    double right = values.back();
    values.pop_back();
    double left = values.back();
    values.pop_back();

    double result = 0.0;
    if (op == Kind::Plus) {
        result = left + right;
    } else if (op == Kind::Minus) {
        result = left - right;
    } else if (op == Kind::Times) {
        result = left * right;
    } else if (op == Kind::Divide) {
        if (right == 0.0) {
            *error = "division by zero";
            return false;
        }
        result = left / right;
    } else {
        *error = "malformed expression";
        return false;
    }

    values.push_back(result);
    return true;
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error) {
    std::vector<double> values;
    std::vector<Kind> ops;

    for (const CalcToken& token : tokens) {
        if (token.kind == Kind::Number) {
            values.push_back(token.value);
        } else if (token.kind == Kind::LParen) {
            ops.push_back(Kind::LParen);
        } else if (token.kind == Kind::RParen) {
            while (!ops.empty() && ops.back() != Kind::LParen) {
                if (!apply_top(values, ops, error)) return false;
            }
            if (ops.empty()) {
                *error = "unmatched ')'";
                return false;
            }
            ops.pop_back();
        } else {
            while (!ops.empty() && ops.back() != Kind::LParen &&
                   precedence(ops.back()) >= precedence(token.kind)) {
                if (!apply_top(values, ops, error)) return false;
            }
            ops.push_back(token.kind);
        }
    }

    while (!ops.empty()) {
        if (ops.back() == Kind::LParen) {
            *error = "unmatched '('";
            return false;
        }
        if (!apply_top(values, ops, error)) return false;
    }

    if (values.size() != 1) {
        *error = "malformed expression";
        return false;
    }
    *out = values.back();
    return true;
}
