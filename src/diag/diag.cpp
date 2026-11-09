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
    // @@EXPECT@@
    {nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, 0, 0},
};

struct Pair {
    const char* bad;
    const char* note;
};
const Pair kWords[] = {
    // @@WORD@@
    {nullptr, nullptr},
};
const Pair kTypes[] = {
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
