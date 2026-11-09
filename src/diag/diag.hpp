#pragma once
#include <string>

namespace relic::diag {

// Extra placement for a diagnostic. Empty hint and an end column of 0
// leave the text identical to a one-column caret.
struct Note {
    int end_col = 0;    // inclusive; 0 or less than col means a single caret
    std::string hint;   // printed as "hint: ..." after the caret line
};

// Formats an error as "path:line:col: error: message", then the source
// line with a caret under the column when the file can be read.
// A Note with an end column draws that many carets, and a hint adds a line.
std::string format(const std::string& path, int line, int col, const std::string& msg,
                   const char* kind = "error");
std::string format(const std::string& path, int line, int col, const std::string& msg,
                   const Note& note, const char* kind = "error");

// A character that is not a token, when it is a common mistake from another
// language. extra is how many columns past the first the span covers.
// next is the following source byte, or 0. Returns null when nothing matches.
struct LexHit {
    const char* msg;
    const char* hint;
    int extra;
};
const LexHit* lex_mistake(const std::string& grapheme, char next);

// A parse failure that is a common mistake. msg null keeps the usual
// "expected ..., got ..." text. cover_prev starts the span at the previous
// token; cover_got underlines the whole current token.
struct ExpectHit {
    const char* msg;
    const char* hint;
    bool cover_prev;
    bool cover_got;
};
bool expect_mistake(const std::string& what, int got_kind, const std::string& got_text, bool has_prev,
                    int prev_kind, const std::string& prev_text, ExpectHit& out);

// The name that started a statement, when the next token cannot follow it.
// "func foo" and "print x" land here. Null when the name is ordinary.
const char* word_mistake(const std::string& word);

// Unknown `as` type that is one step from a real type. hint is the line to print.
bool type_mistake(const std::string& name, const char*& hint);

// Parenthetical for an undefined name (without the parentheses), or null.
// Used only when "did you mean" found nothing.
const char* name_mistake(const std::string& name);

}  // namespace relic::diag
