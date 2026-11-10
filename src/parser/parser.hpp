#pragma once
#include "ast/ast.hpp"
#include "lexer/token.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace relic {

struct ParseError : std::runtime_error {
    int line;
    int col;
    int end_col;
    std::string hint;
    ParseError(int l, int c, const std::string& msg, int end = 0, std::string hint_text = {})
        : std::runtime_error(msg), line(l), col(c), end_col(end > c ? end : c), hint(std::move(hint_text)) {}
};

// Builds a Program tree from lexer tokens. Throws ParseError on bad syntax.
AstPtr parse(const std::vector<Token>& tokens);

}  // namespace relic
