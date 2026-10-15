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
    ParseError(int l, int c, const std::string& msg) : std::runtime_error(msg), line(l), col(c) {}
};

// Builds a Program tree from lexer tokens. Throws ParseError on bad syntax.
AstPtr parse(const std::vector<Token>& tokens);

}  // namespace relic
