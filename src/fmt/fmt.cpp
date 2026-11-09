#include "fmt/fmt.hpp"

#include "diag/diag.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <cctype>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace relic {
namespace {

enum class Ak { Word, Number, Str, Op };

struct Atom {
    Ak kind = Ak::Op;
    std::string text;
    bool unary = false;    // a '-' that negates, not one that subtracts
    bool tight = false;    // a '/' that separates a `use` path
    bool comment = false;  // text is the comment body, without '#'
};

struct Line {
    int indent = 0;
    int depth = 0;  // brackets open at the start of this line
    bool blank = false;
    bool comment_only = false;
    std::vector<Atom> atoms;
};

struct ScanError : std::runtime_error {
    int line;
    int col;
    ScanError(int l, int c, const std::string& m) : std::runtime_error(m), line(l), col(c) {}
};

// Keywords that take a following expression or name, so a space belongs after them.
bool space_kw(const std::string& w) {
    return w == "let" || w == "fun" || w == "if" || w == "else" || w == "while" || w == "for" ||
           w == "return" || w == "not" || w == "and" || w == "or" || w == "in" || w == "use" ||
           w == "try" || w == "catch" || w == "as";
}

bool is_binop(const std::string& t) {
    return t == "+" || t == "-" || t == "*" || t == "/" || t == "%" || t == "==" || t == "!=" ||
           t == "<" || t == "<=" || t == ">" || t == ">=" || t == "and" || t == "or" || t == "in";
}

bool opens_interp(const Atom& a) { return a.kind == Ak::Str && !a.text.empty() && a.text.back() == '{'; }

bool closes_interp(const Atom& b) { return b.kind == Ak::Str && !b.text.empty() && b.text.front() == '}'; }

// A value a binary operator can follow. A '-' after one of these subtracts.
bool ends_expr(const Atom& a) {
    if (a.unary && a.text == "-") return false;
    if (a.kind == Ak::Number) return true;
    if (a.kind == Ak::Str) return a.text.empty() || a.text.back() != '{';
    if (a.kind == Ak::Word) return !space_kw(a.text) && a.text != "let" && a.text != "fun" &&
                                  a.text != "if" && a.text != "else" && a.text != "while" &&
                                  a.text != "for" && a.text != "break" && a.text != "next";
    return a.text == ")" || a.text == "]" || a.text == "}";
}

bool space_between(const Atom& a, const Atom& b) {
    // `{` `{` would become `{{` (an escaped brace) and `}` `}` would become `}}`.
    // One space keeps the interpolation and the map braces apart.
    if (opens_interp(a)) return !b.text.empty() && b.text.front() == '{';
    if (closes_interp(b)) return !a.text.empty() && a.text.back() == '}';
    const std::string& A = a.text;
    const std::string& B = b.text;
    if (B == "." || B == "," || B == ":" || B == ")" || B == "]" || B == "}") return false;
    // `1..2` and `a..b` stay tight. `use ../a` keeps the space after `use`.
    if (B == "..") return a.kind == Ak::Word && space_kw(A);
    if (A == "." || A == ".." || A == "(" || A == "[" || A == "{") return false;
    if (a.tight || b.tight) return false;
    // `--x` is two negations. A space keeps them from looking like one token.
    if (a.unary && A == "-") return b.unary && B == "-";
    if (A == "," || A == ":" || A == "=") return true;
    if (B == "=") return true;
    if (is_binop(A) && !(a.unary && A == "-")) return true;
    if (is_binop(B) && !(b.unary && B == "-")) return true;
    if (b.unary && B == "-") return !(A == "(" || A == "[" || A == "{");
    if (a.kind == Ak::Word && space_kw(A)) return true;
    if (B == "(" || B == "[") return false;
    return true;
}

class Scanner {
public:
    explicit Scanner(const std::string& s) : src_(s) {}

    std::vector<Line> run() {
        while (!eof()) {
            if (at_line_start_) {
                if (!start_line()) break;
                continue;
            }
            skip_spaces();
            if (eof()) {
                finish();
                break;
            }
            char c = peek();
            if (c == '\n') {
                advance();
                finish();
                continue;
            }
            if (c == '#') {
                trailing_comment();
                finish();
                continue;
            }
            code_token();
        }
        if (!cur_.atoms.empty() || cur_open_) finish();
        return std::move(lines_);
    }

private:
    const std::string& src_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    int depth_ = 0;
    bool at_line_start_ = true;
    bool use_path_ = false;
    bool cur_open_ = false;
    Line cur_;
    std::optional<Atom> prev_;
    std::vector<Line> lines_;

    [[noreturn]] void error(const std::string& m) { throw ScanError(line_, col_, m); }
    bool eof() const { return pos_ >= src_.size(); }
    char peek(size_t ahead = 0) const { return pos_ + ahead < src_.size() ? src_[pos_ + ahead] : '\0'; }
    char advance() {
        char c = src_[pos_++];
        if (c == '\n') {
            ++line_;
            col_ = 1;
        } else if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++col_;
        }
        return c;
    }

    void skip_spaces() {
        while (peek() == ' ' || peek() == '\r') advance();
        if (peek() == '\t') error("tabs are not allowed; use 4 spaces");
    }

    // Leading whitespace, blank lines and whole-line comments. False at end of file
    // with nothing left to emit.
    bool start_line() {
        int ind = 0;
        while (peek() == ' ') {
            advance();
            ++ind;
        }
        if (peek() == '\t') error("tabs are not allowed; use 4 spaces");
        if (peek() == '\r') {
            advance();
            return true;
        }
        if (eof()) return false;
        if (peek() == '\n') {
            advance();
            push_blank();
            return true;
        }
        if (peek() == '#') {
            Line ln;
            ln.indent = ind;
            ln.depth = depth_;
            ln.comment_only = true;
            ln.atoms.push_back(comment_atom());
            lines_.push_back(std::move(ln));
            if (peek() == '\r') advance();
            if (peek() == '\n') advance();
            return true;
        }
        cur_ = Line();
        cur_.indent = ind;
        cur_.depth = depth_;
        cur_open_ = true;
        at_line_start_ = false;
        return true;
    }

    Atom comment_atom() {
        advance();  // '#'
        size_t start = pos_;
        while (!eof() && peek() != '\n' && peek() != '\r') advance();
        std::string body = src_.substr(start, pos_ - start);
        while (!body.empty() && (body.back() == ' ' || body.back() == '\t')) body.pop_back();
        Atom c;
        c.comment = true;
        c.text = std::move(body);
        return c;
    }

    void trailing_comment() {
        cur_.atoms.push_back(comment_atom());
        if (peek() == '\r') advance();
        if (peek() == '\n') advance();
    }

    void push_blank() {
        Line ln;
        ln.blank = true;
        ln.depth = depth_;
        lines_.push_back(std::move(ln));
        at_line_start_ = true;
    }

    void finish() {
        if (cur_open_ || !cur_.atoms.empty()) lines_.push_back(std::move(cur_));
        cur_ = Line();
        cur_open_ = false;
        at_line_start_ = true;
        if (depth_ == 0) {
            prev_.reset();
            use_path_ = false;
        }
    }

    void emit(Atom a) {
        if (a.kind == Ak::Op) {
            if (a.text == "(" || a.text == "[" || a.text == "{") ++depth_;
            else if (a.text == ")" || a.text == "]" || a.text == "}") --depth_;
        }
        if (a.kind == Ak::Word && a.text == "use") use_path_ = true;
        if (a.kind == Ak::Word && a.text == "as") use_path_ = false;
        prev_ = a;
        cur_.atoms.push_back(std::move(a));
    }

    void code_token() {
        char c = peek();
        if (std::isdigit(static_cast<unsigned char>(c))) return number();
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') return name();
        if (c == '"') return string();
        op();
    }

    void number() {
        size_t start = pos_;
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
            advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            size_t k = (peek(1) == '+' || peek(1) == '-') ? 2 : 1;
            if (std::isdigit(static_cast<unsigned char>(peek(k)))) {
                for (size_t j = 0; j < k; ++j) advance();
                while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
            }
        }
        if (std::isalpha(static_cast<unsigned char>(peek())) || peek() == '_')
            error("letter right after a number");
        Atom a;
        a.kind = Ak::Number;
        a.text = src_.substr(start, pos_ - start);
        emit(std::move(a));
    }

    void name() {
        size_t start = pos_;
        while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') advance();
        Atom a;
        a.kind = Ak::Word;
        a.text = src_.substr(start, pos_ - start);
        emit(std::move(a));
    }

    void op() {
        char ch = advance();
        auto two = [&](char next, const char* yes, const char* no) {
            Atom a;
            a.kind = Ak::Op;
            if (peek() == next) {
                advance();
                a.text = yes;
            } else {
                a.text = no;
            }
            emit(std::move(a));
        };
        switch (ch) {
            case '+': emit_simple("+"); return;
            case '*': emit_simple("*"); return;
            case '%': emit_simple("%"); return;
            case ',': emit_simple(","); return;
            case ':': emit_simple(":"); return;
            case '/': {
                Atom a;
                a.kind = Ak::Op;
                a.text = "/";
                a.tight = use_path_;
                emit(std::move(a));
                return;
            }
            case '-': {
                Atom a;
                a.kind = Ak::Op;
                a.text = "-";
                a.unary = !prev_ || !ends_expr(*prev_);
                emit(std::move(a));
                return;
            }
            case '=': two('=', "==", "="); return;
            case '<': two('=', "<=", "<"); return;
            case '>': two('=', ">=", ">"); return;
            case '.': two('.', "..", "."); return;
            case '!':
                if (peek() == '=') {
                    advance();
                    emit_simple("!=");
                    return;
                }
                error("unexpected '!'");
            case '(': emit_simple("("); return;
            case ')': emit_simple(")"); return;
            case '[': emit_simple("["); return;
            case ']': emit_simple("]"); return;
            case '{': emit_simple("{"); return;
            case '}': emit_simple("}"); return;
            default: error("unexpected character");
        }
    }

    void emit_simple(const char* text) {
        Atom a;
        a.kind = Ak::Op;
        a.text = text;
        emit(std::move(a));
    }

    // One string, split so an interpolation's expression can be spaced on its own.
    // Literal pieces keep the source bytes, escapes included.
    void string() {
        std::string chunk;
        chunk += advance();  // opening '"'
        while (true) {
            if (eof() || peek() == '\n') error("unterminated string");
            char ch = peek();
            if (ch == '\\') {
                chunk += advance();
                if (eof()) error("unterminated string");
                char e = advance();
                chunk += e;
                if (e == 'u') {
                    if (peek() != '{') error("bad unicode escape");
                    chunk += advance();
                    while (std::isxdigit(static_cast<unsigned char>(peek()))) chunk += advance();
                    if (peek() != '}') error("bad unicode escape");
                    chunk += advance();
                }
                continue;
            }
            if (ch == '"') {
                chunk += advance();
                Atom a;
                a.kind = Ak::Str;
                a.text = std::move(chunk);
                emit(std::move(a));
                return;
            }
            if (ch == '{') {
                if (peek(1) == '{') {
                    chunk += advance();
                    chunk += advance();
                    continue;
                }
                chunk += advance();
                Atom a;
                a.kind = Ak::Str;
                a.text = std::move(chunk);
                emit(std::move(a));
                // Expression tokens up to the '}' that closes this interpolation.
                // That '}' is part of the next literal piece, so it is consumed once.
                int braces = 0;
                while (true) {
                    skip_spaces();
                    if (eof() || peek() == '\n') error("unterminated string");
                    if (peek() == '}' && braces == 0) {
                        chunk = "}";
                        advance();
                        break;
                    }
                    if (peek() == '#') error("comment inside a string");
                    size_t before = cur_.atoms.size();
                    code_token();
                    if (cur_.atoms.size() > before) {
                        const std::string& t = cur_.atoms.back().text;
                        if (t == "{") ++braces;
                        else if (t == "}" && braces > 0) --braces;
                    }
                }
                continue;
            }
            if (ch == '}' && peek(1) == '}') {
                chunk += advance();
                chunk += advance();
                continue;
            }
            chunk += advance();
        }
    }
};

bool is_comment(const Atom& a) { return a.comment; }

void canon_indents(std::vector<Line>& lines) {
    int stmt = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        Line& ln = lines[i];
        if (ln.blank) continue;
        if (!ln.comment_only && ln.depth == 0) stmt = ln.indent;
        if (ln.comment_only) {
            int canon = stmt;
            for (size_t j = i + 1; j < lines.size(); ++j) {
                if (lines[j].blank || lines[j].comment_only) continue;
                if (lines[j].depth == 0) canon = lines[j].indent;
                else if (lines[j].indent % 4 == 0) canon = lines[j].indent;
                else canon = stmt + 4;
                break;
            }
            if (canon < 0) canon = 0;
            // A comment that is not on a 4-space column, or that sits to the left of
            // the next line of code, lines up with that line. Deeper comments stay.
            if (ln.indent % 4 != 0 || ln.indent < canon) ln.indent = canon;
        } else if (!ln.comment_only && ln.depth > 0 && ln.indent % 4 != 0) {
            ln.indent = stmt + 4;
        }
        if (ln.indent < 0) ln.indent = 0;
    }
}

std::vector<Line> squeeze(std::vector<Line> lines) {
    std::vector<Line> out;
    bool prev_blank = true;  // drops blanks at the start
    for (Line& ln : lines) {
        if (ln.blank) {
            if (prev_blank) continue;
            prev_blank = true;
            out.push_back(std::move(ln));
        } else {
            prev_blank = false;
            out.push_back(std::move(ln));
        }
    }
    while (!out.empty() && out.back().blank) out.pop_back();
    return out;
}

std::string print_lines(const std::vector<Line>& lines) {
    if (lines.empty()) return "";
    std::string s;
    for (const Line& ln : lines) {
        if (ln.blank) {
            s += '\n';
            continue;
        }
        s.append(static_cast<size_t>(ln.indent), ' ');
        const Atom* prev = nullptr;
        bool any = false;
        for (const Atom& a : ln.atoms) {
            if (is_comment(a)) {
                if (any) s += "  ";
                s += '#';
                s += a.text;
                continue;
            }
            if (prev && space_between(*prev, a)) s += ' ';
            s += a.text;
            prev = &a;
            any = true;
        }
        s += '\n';
    }
    return s;
}

}  // namespace

FmtResult format_source(const std::string& path, const std::string& source) {
    FmtResult r;
    try {
        parse(lex(source));
    } catch (const LexError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
        return r;
    } catch (const ParseError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
        return r;
    }
    try {
        auto lines = Scanner(source).run();
        canon_indents(lines);
        r.out = print_lines(squeeze(std::move(lines)));
    } catch (const ScanError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
    }
    return r;
}

}  // namespace relic
