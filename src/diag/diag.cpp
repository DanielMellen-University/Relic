#include "diag/diag.hpp"

#include "lexer/token.hpp"

#include <fstream>

namespace relic::diag {

// The source line an error points at, with carets under the columns:
//     print(cuont)
//           ^
// end_col draws through that column (one caret when it is col).
// Empty when the file cannot be read or the line is not there.
std::string snippet(const std::string& path, int line, int col, int end_col = 0) {
    if (line < 1 || col < 1) return "";
    if (end_col < col) end_col = col;
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::string text;
    for (int i = 0; i < line; ++i)
        if (!std::getline(f, text)) return "";
    if (!text.empty() && text.back() == '\r') text.pop_back();
    if (text.find_first_not_of(" \t") == std::string::npos) return "";
    // Very long lines are cut to a window around the column. Columns count
    // characters, so the window start is found by walking that many
    // characters in, and the column is moved by the characters cut off.
    // The window itself stays 160 bytes, stepped to character edges.
    if (text.size() > 160) {
        size_t skip = static_cast<size_t>(col) > 80 ? static_cast<size_t>(col) - 80 : 0;
        size_t from = 0;
        size_t cut = 0;
        while (from < text.size() && cut < skip) {
            unsigned char c = static_cast<unsigned char>(text[from]);
            size_t len = (c & 0x80) == 0 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
            if (from + len > text.size()) len = text.size() - from;
            from += len;
            ++cut;
        }
        size_t to = from + 160;
        while (to < text.size() && (static_cast<unsigned char>(text[to]) & 0xC0) == 0x80) ++to;
        text = text.substr(from, to - from);
        col -= static_cast<int>(cut);
        end_col -= static_cast<int>(cut);
        if (end_col < col) end_col = col;
    }
    std::string pad;
    // Columns count characters: one pad space per character before the column.
    for (size_t i = 0, n = 0; n + 1 < static_cast<size_t>(col) && i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c & 0xC0) == 0x80) continue;
        pad += c == '\t' ? '\t' : ' ';
        ++n;
    }
    int marks = end_col - col + 1;
    if (marks < 1) marks = 1;
    if (marks > 200) marks = 200;
    return "    " + text + "\n    " + pad + std::string(static_cast<size_t>(marks), '^') + "\n";
}

std::string format(const std::string& path, int line, int col, const std::string& msg, const Note& note,
                   const char* kind) {
    std::string out = path + ":" + std::to_string(line) + ":" + std::to_string(col) + ": " + kind + ": " + msg +
                      "\n" + snippet(path, line, col, note.end_col);
    if (!note.hint.empty()) out += "hint: " + note.hint + "\n";
    return out;
}

std::string format(const std::string& path, int line, int col, const std::string& msg, const char* kind) {
    return format(path, line, col, msg, Note{}, kind);
}

namespace {

struct LexRow {
    const char* g;
    char next;
    const char* msg;
    const char* hint;
    int extra;
};
const LexRow kLex[] = {
    {"&", '&', "unexpected '&&'", "Relic uses 'and', not '&&'", 1},
    {"&", '\0', "unexpected character '&'", "Relic uses 'and', not '&'", 0},
    {"|", '|', "unexpected '||'", "Relic uses 'or', not '||'", 1},
    {"|", '\0', "unexpected character '|'", "Relic uses 'or', not '|'", 0},
    {";", '\0', "unexpected character ';'", "statements end at a newline; delete the semicolon", 0},
    {"'", '\0', "single quotes are not strings", "strings use straight double quotes", 0},
    {"`", '\0', "backticks are not strings", "strings use straight double quotes, not backticks", 0},
    {"~", '\0', "unexpected character '~'", "write 'not' to negate a boolean", 0},
    {"?", '\0', "unexpected character '?'", "Relic has no ternary operator; use if and else", 0},
    {"@", '\0', "unexpected character '@'", "Relic does not use '@'", 0},
    {"^", '\0', "unexpected character '^'", "Relic has no xor operator", 0},
    {"\x5c", '\0', "a backslash is not an operator", "escapes belong inside a string", 0},
    {"\xe2\x80\x93", '\0', "unexpected character '\xe2\x80\x93'", "this dash is not a minus sign; use '-'", 0},
    {"\xe2\x80\x94", '\0', "unexpected character '\xe2\x80\x94'", "this dash is not a minus sign; use '-'", 0},
    {"\xe2\x88\x92", '\0', "unexpected character '\xe2\x88\x92'", "this minus sign is not '-'; use '-'", 0},
    {"\xc3\x97", '\0', "unexpected character '\xc3\x97'", "multiplication is '*'", 0},
    {"\xc3\xb7", '\0', "unexpected character '\xc3\xb7'", "division is '/'", 0},
    {"\xe2\x89\xa0", '\0', "unexpected character '\xe2\x89\xa0'", "write '!=' to test inequality", 0},
    {"\xe2\x89\xa4", '\0', "unexpected character '\xe2\x89\xa4'", "write '<='", 0},
    {"\xe2\x89\xa5", '\0', "unexpected character '\xe2\x89\xa5'", "write '>='", 0},
    {"\xe2\x80\xa6", '\0', "unexpected character '\xe2\x80\xa6'", "a range is written '..'", 0},
    {"\xe2\x86\x92", '\0', "unexpected character '\xe2\x86\x92'", "Relic has no arrow character", 0},
    {"\xc2\xac", '\0', "unexpected character '\xc2\xac'", "write 'not'", 0},
    {"\xe2\x88\xa7", '\0', "unexpected character '\xe2\x88\xa7'", "write 'and'", 0},
    {"\xe2\x88\xa8", '\0', "unexpected character '\xe2\x88\xa8'", "write 'or'", 0},
    // @@LEX@@
    {nullptr, 0, nullptr, nullptr, 0},
};

struct ExpectRow {
    const char* what;
    int got;
    const char* got_text;
    int prev;
    const char* prev_text;
    const char* msg;
    const char* hint;
    int cover_prev;
    int cover_got;
};
const ExpectRow kExpect[] = {
    {"an expression", (int)relic::Tok::Assign, "", (int)relic::Tok::Plus, "", "Relic has no '+='", "write x = x + 1", 1, 1},
    {"an expression", (int)relic::Tok::Assign, "", (int)relic::Tok::Minus, "", "Relic has no '-='", "write x = x - 1", 1, 1},
    {"an expression", (int)relic::Tok::Assign, "", (int)relic::Tok::Star, "", "Relic has no '*='", "write x = x * 1", 1, 1},
    {"an expression", (int)relic::Tok::Assign, "", (int)relic::Tok::Slash, "", "Relic has no '/='", "write x = x / 1", 1, 1},
    {"an expression", (int)relic::Tok::Assign, "", (int)relic::Tok::Percent, "", "Relic has no '%='", "write x = x % 1", 1, 1},
    {"an expression", (int)relic::Tok::Assign, "", (int)relic::Tok::EqEq, "", "Relic has no '==='", "compare with '=='; '=' assigns", 1, 1},
    {"an expression", (int)relic::Tok::Assign, "", (int)relic::Tok::BangEq, "", "Relic has no '!=='", "compare with '!='", 1, 1},
    {"an expression", (int)relic::Tok::Greater, "", (int)relic::Tok::Minus, "", "Relic has no '->'", "there is no arrow; write the steps out", 1, 1},
    {"an expression", (int)relic::Tok::Greater, "", (int)relic::Tok::Assign, "", "Relic has no '=>'", "a function is written 'fun name() = ...'", 1, 1},
    {"end of line", (int)relic::Tok::Colon, "", -1, "", "", "Relic statements do not end with a colon", 0, 1},
    {"an expression", (int)relic::Tok::Slash, "", (int)relic::Tok::Slash, "", "comments start with '#'", "a comment runs from '#' to the end of the line", 1, 1},
    {"an expression", (int)relic::Tok::Star, "", (int)relic::Tok::Slash, "", "Relic comments start with '#', not '/*'", "delete the '/*'", 1, 1},
    {"an expression", (int)relic::Tok::Plus, "", (int)relic::Tok::Plus, "", "Relic has no '++'", "write the new value out, as in n = n + 1", 1, 1},
    {"end of line before the block", (int)relic::Tok::Colon, "", -1, "", "", "Relic headers do not end with a colon; indent the next line 4 spaces", 0, 1},
    {"end of line before the block", (int)relic::Tok::Assign, "", -1, "", "", "compare with '=='; a single '=' assigns", 0, 1},
    {"'=' after the name", (int)relic::Tok::Colon, "", -1, "", "", "use '=', not ':='", 0, 1},
    {"'in' after the loop name", (int)relic::Tok::Name, "of", -1, "", "", "loops are written 'for name in list'", 0, 1},
    {"'in' after the loop name", (int)relic::Tok::Colon, "", -1, "", "", "loops are written 'for name in list', not a colon", 0, 1},
    {"end of line", (int)relic::Tok::Not, "", -1, "", "", "write 'not (a in b)' to negate a test", 0, 1},
    {"'catch' after the 'try' block", -1, "", -1, "", "", "a try needs a catch after its block", 0, 1},
    // @@EXPECT@@
    {nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, 0, 0},
};

struct Pair {
    const char* bad;
    const char* note;
};
const Pair kWords[] = {
    {"func", "functions are written with 'fun', not 'func'"},
    {"function", "functions are written with 'fun', not 'function'"},
    {"def", "functions are written with 'fun', not 'def'"},
    {"fn", "functions are written with 'fun', not 'fn'"},
    {"elseif", "write 'else if', not 'elseif'"},
    {"elif", "write 'else if', not 'elif'"},
    {"elsif", "write 'else if', not 'elsif'"},
    {"var", "bindings start with 'let', not 'var'"},
    {"const", "bindings start with 'let'; Relic has no const"},
    {"int", "write 'let name = ... as int', not a C declaration"},
    {"string", "write 'let name = ... as string'"},
    {"bool", "write 'let name = ... as bool'"},
    {"float", "write 'let name = ... as float'"},
    {"import", "modules are loaded with 'use', not 'import'"},
    {"print", "call it as print(...); the parentheses are required"},
    {"len", "call it as len(...); the parentheses are required"},
    {"class", "Relic has no classes; use a map or a module"},
    {"switch", "branch with if and else if"},
    {"lambda", "write 'fun name() = ...'"},
    {"except", "catch an error with try and catch"},
    // @@WORD@@
    {nullptr, nullptr},
};
const Pair kTypes[] = {
    {"integer", "did you mean 'int'?"},
    {"str", "did you mean 'string'?"},
    {"boolean", "did you mean 'bool'?"},
    {"dict", "did you mean 'map'?"},
    {"array", "did you mean 'list'?"},
    {"void", "did you mean 'none'?"},
    {"nil", "did you mean 'none'?"},
    {"double", "did you mean 'float'?"},
    {"i64", "did you mean 'int'?"},
    // @@TYPE@@
    {nullptr, nullptr},
};
const Pair kNames[] = {
    // @@NAME@@
    {nullptr, nullptr},
};

const char* pair_note(const Pair* rows, const std::string& name) {
    for (const Pair* p = rows; p->bad; ++p)
        if (name == p->bad) return p->note;
    return nullptr;
}

}  // namespace

const LexHit* lex_mistake(const std::string& grapheme, char next) {
    static LexHit found;
    const LexRow* fallback = nullptr;
    for (const LexRow& r : kLex) {
        if (!r.g) break;
        if (grapheme != r.g) continue;
        if (r.next != 0) {
            if (next == r.next) {
                found = LexHit{r.msg, r.hint, r.extra};
                return &found;
            }
        } else if (!fallback) {
            fallback = &r;
        }
    }
    if (!fallback) return nullptr;
    found = LexHit{fallback->msg, fallback->hint, fallback->extra};
    return &found;
}

bool expect_mistake(const std::string& what, int got_kind, const std::string& got_text, bool has_prev, int prev_kind,
                    const std::string& prev_text, ExpectHit& out) {
    for (const ExpectRow& r : kExpect) {
        if (!r.what) break;
        if (what != r.what) continue;
        if (r.got >= 0 && got_kind != r.got) continue;
        if (r.got_text && r.got_text[0] && got_text != r.got_text) continue;
        if (r.prev >= 0) {
            if (!has_prev || prev_kind != r.prev) continue;
            if (r.prev_text && r.prev_text[0] && prev_text != r.prev_text) continue;
        }
        out.msg = (r.msg && r.msg[0]) ? r.msg : nullptr;
        out.hint = (r.hint && r.hint[0]) ? r.hint : nullptr;
        out.cover_prev = r.cover_prev != 0;
        out.cover_got = r.cover_got != 0;
        return true;
    }
    return false;
}

const char* word_mistake(const std::string& word) { return pair_note(kWords, word); }

bool type_mistake(const std::string& name, const char*& hint) {
    hint = pair_note(kTypes, name);
    return hint != nullptr;
}

const char* name_mistake(const std::string& name) { return pair_note(kNames, name); }

}  // namespace relic::diag
