#pragma once
#include <string>

namespace relic {

enum class Tok {
    // literals and names
    Number, String, Name,
    // interpolated strings: "a {x} b" -> StringPart("a ") InterpStart Name(x) InterpEnd StringEnd(" b")
    StringPart, InterpStart, InterpEnd, StringEnd,
    // keywords
    Let, Fun, If, Else, While, For, In, Break, Next, Return,
    And, Or, Not, True, False, None, Use,
    // operators and punctuation
    Plus, Minus, Star, Slash, Percent,
    EqEq, BangEq, Less, LessEq, Greater, GreaterEq,
    Assign, Dot, DotDot, Comma, Colon,
    LParen, RParen, LBracket, RBracket, LBrace, RBrace,
    // layout
    Newline, Indent, Dedent, Eof,
};

struct Token {
    Tok kind;
    std::string text;  // source text, or the decoded value for strings
    int line;
    int col;
};

const char* tok_name(Tok kind);

}  // namespace relic
