#include "lexer/lexer.hpp"

#include <cctype>
#include <unordered_map>

namespace relic {

const char* tok_name(Tok k) {
    switch (k) {
        case Tok::Number: return "NUMBER";
        case Tok::String: return "STRING";
        case Tok::Name: return "NAME";
        case Tok::StringPart: return "STRING_PART";
        case Tok::InterpStart: return "INTERP_START";
        case Tok::InterpEnd: return "INTERP_END";
        case Tok::StringEnd: return "STRING_END";
        case Tok::Let: return "LET";
        case Tok::Fun: return "FUN";
        case Tok::If: return "IF";
        case Tok::Else: return "ELSE";
        case Tok::While: return "WHILE";
        case Tok::For: return "FOR";
        case Tok::In: return "IN";
        case Tok::Break: return "BREAK";
        case Tok::Next: return "NEXT";
        case Tok::Return: return "RETURN";
        case Tok::And: return "AND";
        case Tok::Or: return "OR";
        case Tok::Not: return "NOT";
        case Tok::True: return "TRUE";
        case Tok::False: return "FALSE";
        case Tok::None: return "NONE";
        case Tok::Plus: return "PLUS";
        case Tok::Minus: return "MINUS";
        case Tok::Star: return "STAR";
        case Tok::Slash: return "SLASH";
        case Tok::Percent: return "PERCENT";
        case Tok::EqEq: return "EQ_EQ";
        case Tok::BangEq: return "BANG_EQ";
        case Tok::Less: return "LESS";
        case Tok::LessEq: return "LESS_EQ";
        case Tok::Greater: return "GREATER";
        case Tok::GreaterEq: return "GREATER_EQ";
        case Tok::Assign: return "ASSIGN";
        case Tok::Dot: return "DOT";
        case Tok::DotDot: return "DOT_DOT";
        case Tok::Comma: return "COMMA";
        case Tok::Colon: return "COLON";
        case Tok::LParen: return "LPAREN";
        case Tok::RParen: return "RPAREN";
        case Tok::LBracket: return "LBRACKET";
        case Tok::RBracket: return "RBRACKET";
        case Tok::LBrace: return "LBRACE";
        case Tok::RBrace: return "RBRACE";
        case Tok::Newline: return "NEWLINE";
        case Tok::Indent: return "INDENT";
        case Tok::Dedent: return "DEDENT";
        case Tok::Eof: return "EOF";
    }
    return "?";
}

namespace {

const std::unordered_map<std::string, Tok> kKeywords = {
    {"let", Tok::Let},     {"fun", Tok::Fun},       {"if", Tok::If},
    {"else", Tok::Else},   {"while", Tok::While},   {"for", Tok::For},
    {"in", Tok::In},       {"break", Tok::Break},   {"next", Tok::Next},
    {"return", Tok::Return}, {"and", Tok::And},     {"or", Tok::Or},
    {"not", Tok::Not},     {"true", Tok::True},     {"false", Tok::False},
    {"none", Tok::None},
};

constexpr int kIndentWidth = 4;

class Lexer {
public:
    explicit Lexer(const std::string& s) : src_(s) {}

    std::vector<Token> run() {
        bool at_line_start = true;
        while (true) {
            if (at_line_start && bracket_depth() == 0) {
                if (!layout()) break;  // reached end of file
                at_line_start = false;
            }
            if (eof()) break;
            char c = peek();
            if (c == ' ' || c == '\r') { advance(); continue; }
            if (c == '\t') error(line_, col_, "tabs are not allowed; use 4 spaces");
            if (c == '#') { skip_comment(); continue; }
            if (c == '\n') {
                if (!interp_.empty()) error(interp_.back().line, interp_.back().col, "unterminated string");
                advance();
                if (bracket_depth() == 0) {
                    emit(Tok::Newline, "", line_ - 1, last_line_end_col_);
                    at_line_start = true;
                }
                continue;
            }
            token();
        }
        if (!interp_.empty()) error(interp_.back().line, interp_.back().col, "unterminated string");
        if (!brackets_.empty())
            error(brackets_.back().line, brackets_.back().col,
                  std::string("unclosed '") + brackets_.back().ch + "'");
        if (!out_.empty() && out_.back().kind != Tok::Newline && out_.back().kind != Tok::Dedent)
            emit(Tok::Newline, "", line_, col_);
        while (indents_.size() > 1) { indents_.pop_back(); emit(Tok::Dedent, "", line_, col_); }
        emit(Tok::Eof, "", line_, col_);
        return out_;
    }

private:
    struct Open { char ch; int line; int col; };
    struct Interp { int depth; int line; int col; };

    const std::string& src_;
    size_t pos_ = 0;
    int line_ = 1, col_ = 1, last_line_end_col_ = 1;
    std::vector<int> indents_{0};
    std::vector<Open> brackets_;
    std::vector<Interp> interp_;
    std::vector<Token> out_;

    [[noreturn]] void error(int l, int c, const std::string& m) { throw LexError(l, c, m); }
    bool eof() const { return pos_ >= src_.size(); }
    char peek(size_t ahead = 0) const {
        return pos_ + ahead < src_.size() ? src_[pos_ + ahead] : '\0';
    }
    char advance() {
        char c = src_[pos_++];
        if (c == '\n') { last_line_end_col_ = col_; ++line_; col_ = 1; } else { ++col_; }
        return c;
    }
    void emit(Tok k, std::string text, int l, int c) { out_.push_back({k, std::move(text), l, c}); }
    size_t bracket_depth() const {
        // brackets opened inside an interpolation do not join lines; strings cannot span lines
        return brackets_.size();
    }
    void skip_comment() { while (!eof() && peek() != '\n') advance(); }

    // Handles leading whitespace. Skips blank and comment lines.
    // Returns false at end of file.
    bool layout() {
        while (true) {
            int width = 0;
            while (peek() == ' ' || peek() == '\t') {
                if (peek() == '\t') error(line_, col_, "tabs are not allowed; use 4 spaces");
                advance();
                ++width;
            }
            if (eof()) return false;
            if (peek() == '\r') { advance(); continue; }
            if (peek() == '\n') { advance(); continue; }
            if (peek() == '#') { skip_comment(); continue; }
            if (width == indents_.back()) return true;
            if (width > indents_.back()) {
                if (width != indents_.back() + kIndentWidth)
                    error(line_, col_, "indent must be exactly 4 spaces deeper than the line above");
                if (out_.empty()) error(line_, col_, "unexpected indent");
                indents_.push_back(width);
                emit(Tok::Indent, "", line_, col_);
                return true;
            }
            while (width < indents_.back()) {
                indents_.pop_back();
                emit(Tok::Dedent, "", line_, col_);
            }
            if (width != indents_.back())
                error(line_, col_, "dedent does not line up with any outer block");
            return true;
        }
    }

    void token() {
        int l = line_, c = col_;
        char ch = peek();
        if (std::isdigit(static_cast<unsigned char>(ch))) return number(l, c);
        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') return name(l, c);
        if (ch == '"') { advance(); return string_body(l, c, true); }
        advance();
        auto two = [&](char next, Tok yes, Tok no, const char* ty, const char* tn) {
            if (peek() == next) { advance(); emit(yes, ty, l, c); } else { emit(no, tn, l, c); }
        };
        switch (ch) {
            case '+': emit(Tok::Plus, "+", l, c); return;
            case '-': emit(Tok::Minus, "-", l, c); return;
            case '*': emit(Tok::Star, "*", l, c); return;
            case '/': emit(Tok::Slash, "/", l, c); return;
            case '%': emit(Tok::Percent, "%", l, c); return;
            case ',': emit(Tok::Comma, ",", l, c); return;
            case ':': emit(Tok::Colon, ":", l, c); return;
            case '=': two('=', Tok::EqEq, Tok::Assign, "==", "="); return;
            case '<': two('=', Tok::LessEq, Tok::Less, "<=", "<"); return;
            case '>': two('=', Tok::GreaterEq, Tok::Greater, ">=", ">"); return;
            case '.': two('.', Tok::DotDot, Tok::Dot, "..", "."); return;
            case '!':
                if (peek() == '=') { advance(); emit(Tok::BangEq, "!=", l, c); return; }
                error(l, c, "unexpected '!'; use 'not' for logic");
            case '(': open('(', l, c); emit(Tok::LParen, "(", l, c); return;
            case '[': open('[', l, c); emit(Tok::LBracket, "[", l, c); return;
            case '{':
                if (!interp_.empty()) ++interp_.back().depth;
                else open('{', l, c);
                emit(Tok::LBrace, "{", l, c);
                return;
            case ')': close('(', ')', l, c); emit(Tok::RParen, ")", l, c); return;
            case ']': close('[', ']', l, c); emit(Tok::RBracket, "]", l, c); return;
            case '}':
                if (!interp_.empty()) {
                    if (interp_.back().depth == 0) {
                        interp_.pop_back();
                        emit(Tok::InterpEnd, "}", l, c);
                        return string_body(line_, col_, false);
                    }
                    --interp_.back().depth;
                    emit(Tok::RBrace, "}", l, c);
                    return;
                }
                close('{', '}', l, c);
                emit(Tok::RBrace, "}", l, c);
                return;
            default:
                error(l, c, std::string("unexpected character '") + ch + "'");
        }
    }

    void open(char ch, int l, int c) {
        if (!interp_.empty() ) {
            // inside {...} of a string: track nesting but stay on one line
            ++interp_.back().depth;
            brackets_.push_back({ch, l, c});
            return;
        }
        brackets_.push_back({ch, l, c});
    }
    void close(char want, char got, int l, int c) {
        if (brackets_.empty() || brackets_.back().ch != want)
            error(l, c, std::string("unmatched '") + got + "'");
        brackets_.pop_back();
        if (!interp_.empty() && interp_.back().depth > 0) --interp_.back().depth;
    }

    void number(int l, int c) {
        size_t start = pos_;
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
            advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
        if (std::isalpha(static_cast<unsigned char>(peek())) || peek() == '_')
            error(line_, col_, "letter right after a number");
        emit(Tok::Number, src_.substr(start, pos_ - start), l, c);
    }

    void name(int l, int c) {
        size_t start = pos_;
        while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') advance();
        std::string word = src_.substr(start, pos_ - start);
        auto it = kKeywords.find(word);
        emit(it == kKeywords.end() ? Tok::Name : it->second, word, l, c);
    }

    // Reads string text up to the closing quote or the next interpolation.
    // first: true when this segment starts right after the opening quote.
    void string_body(int l, int c, bool first) {
        std::string text;
        while (true) {
            if (eof() || peek() == '\n') error(l, c, "unterminated string");
            char ch = advance();
            if (ch == '"') {
                emit(first ? Tok::String : Tok::StringEnd, text, l, c);
                return;
            }
            if (ch == '\\') {
                int el = line_, ec = col_ - 1;
                if (eof()) error(l, c, "unterminated string");
                char e = advance();
                switch (e) {
                    case 'n': text += '\n'; break;
                    case 't': text += '\t'; break;
                    case '"': text += '"'; break;
                    case '\\': text += '\\'; break;
                    default: error(el, ec, std::string("unknown escape '\\") + e + "'");
                }
                continue;
            }
            if (ch == '{') {
                if (peek() == '{') { advance(); text += '{'; continue; }
                emit(Tok::StringPart, text, l, c);
                emit(Tok::InterpStart, "{", line_, col_ - 1);
                interp_.push_back({0, l, c});
                return;
            }
            if (ch == '}' && peek() == '}') { advance(); text += '}'; continue; }
            text += ch;
        }
    }
};

}  // namespace

std::vector<Token> lex(const std::string& source) { return Lexer(source).run(); }

}  // namespace relic
