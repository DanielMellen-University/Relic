#pragma once
#include "lexer/token.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace relic {

struct LexError : std::runtime_error {
    int line;
    int col;
    LexError(int l, int c, const std::string& msg) : std::runtime_error(msg), line(l), col(c) {}
};

// Turns Relic source into tokens. Throws LexError on bad input.
// Layout rules (see docs/LANGUAGE.md, Source files):
// - blocks are 4-space indents; tabs in indentation are an error
// - blank and comment-only lines produce no tokens
// - newlines inside ( ), [ ] or { } do not end a line
std::vector<Token> lex(const std::string& source);

}  // namespace relic
