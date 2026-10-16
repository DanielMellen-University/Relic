#include "parser/parser.hpp"

namespace relic {

namespace {

// How a token reads in an error message.
std::string describe(const Token& t) {
    switch (t.kind) {
        case Tok::Newline: return "end of line";
        case Tok::Indent: return "an indented block";
        case Tok::Dedent: return "end of block";
        case Tok::Eof: return "end of file";
        case Tok::Number: return "number " + t.text;
        case Tok::String: case Tok::StringPart: return "a string";
        case Tok::Name: return "name '" + t.text + "'";
        default: return "'" + t.text + "'";
    }
}

AstPtr make(Node kind, const Token& at, std::string text = "") {
    auto n = std::make_unique<Ast>();
    n->kind = kind;
    n->text = std::move(text);
    n->line = at.line;
    n->col = at.col;
    return n;
}

AstPtr make(Node kind, const Ast& at, std::string text = "") {
    auto n = std::make_unique<Ast>();
    n->kind = kind;
    n->text = std::move(text);
    n->line = at.line;
    n->col = at.col;
    return n;
}

class Parser {
public:
    explicit Parser(const std::vector<Token>& t) : toks_(t) {}

    AstPtr program() {
        auto prog = make(Node::Program, peek());
        while (!at(Tok::Eof)) prog->kids.push_back(statement());
        return prog;
    }

private:
    const std::vector<Token>& toks_;
    size_t pos_ = 0;
    // Nesting of expressions, `-`, `not` and blocks. A limit keeps a hostile
    // file from overflowing the C++ stack.
    static constexpr int kMaxNesting = 100;
    int nesting_ = 0;
    struct Nest {
        Parser& p;
        explicit Nest(Parser& pp) : p(pp) {
            if (++p.nesting_ > kMaxNesting)
                p.fail(p.peek(), "too deeply nested (more than " + std::to_string(kMaxNesting) + " levels)");
        }
        ~Nest() { --p.nesting_; }
    };

    const Token& peek(size_t ahead = 0) const {
        size_t i = pos_ + ahead;
        return i < toks_.size() ? toks_[i] : toks_.back();
    }
    bool at(Tok k) const { return peek().kind == k; }
    const Token& advance() {
        const Token& t = peek();
        if (pos_ < toks_.size() - 1) ++pos_;
        return t;
    }
    bool match(Tok k) {
        if (!at(k)) return false;
        advance();
        return true;
    }
    [[noreturn]] void fail(const Token& t, const std::string& msg) {
        throw ParseError(t.line, t.col, msg);
    }
    [[noreturn]] void fail_expected(const std::string& what) {
        fail(peek(), "expected " + what + ", got " + describe(peek()));
    }
    const Token& expect(Tok k, const std::string& what) {
        if (!at(k)) fail_expected(what);
        return advance();
    }
    void end_of_line() { expect(Tok::Newline, "end of line"); }

    // ---- statements ----

    AstPtr statement() {
        const Token& t = peek();
        switch (t.kind) {
            case Tok::Let: return let_stmt();
            case Tok::Fun: return fun_stmt();
            case Tok::If: return if_stmt();
            case Tok::While: return while_stmt();
            case Tok::For: return for_stmt();
            case Tok::Use: return use_stmt();
            case Tok::Try: return try_stmt();
            case Tok::Catch: fail(t, "'catch' without a matching 'try'");
            case Tok::Return: {
                advance();
                auto n = make(Node::Return, t);
                if (!at(Tok::Newline)) n->kids.push_back(expression());
                end_of_line();
                return n;
            }
            case Tok::Break: advance(); end_of_line(); return make(Node::Break, t);
            case Tok::Next: advance(); end_of_line(); return make(Node::Next, t);
            case Tok::Else: fail(t, "'else' without a matching 'if'");
            case Tok::Indent: fail(t, "unexpected indent");
            default: return expr_stmt();
        }
    }

    AstPtr block() {
        Nest nest(*this);
        const Token& start = peek();
        if (!at(Tok::Newline)) fail_expected("end of line before the block");
        advance();
        if (!at(Tok::Indent)) fail_expected("an indented block");
        advance();
        auto b = make(Node::Block, start);
        while (!at(Tok::Dedent) && !at(Tok::Eof)) b->kids.push_back(statement());
        match(Tok::Dedent);
        return b;
    }

    // try <block> catch [name] <block>
    AstPtr try_stmt() {
        const Token& kw = advance();
        auto body = block();
        if (!at(Tok::Catch)) fail_expected("'catch' after the 'try' block");
        advance();
        std::string name;
        if (at(Tok::Name)) name = advance().text;
        auto n = make(Node::Try, kw, name);
        n->kids.push_back(std::move(body));
        n->kids.push_back(block());
        return n;
    }

    // use name, use dir/name, use ../dir/name
    AstPtr use_stmt() {
        const Token& kw = advance();
        std::string path;
        while (at(Tok::DotDot)) {
            advance();
            expect(Tok::Slash, "'/' after '..'");
            path += "../";
        }
        path += expect(Tok::Name, "a module name after 'use'").text;
        while (match(Tok::Slash)) path += "/" + expect(Tok::Name, "a name after '/'").text;
        auto n = make(Node::Use, kw, path);
        if (match(Tok::As)) {
            const Token& name = expect(Tok::Name, "a name after 'as'");
            n->kids.push_back(make(Node::Name, name, name.text));
        }
        end_of_line();
        return n;
    }

    AstPtr let_stmt() {
        const Token& kw = advance();
        const Token& name = expect(Tok::Name, "a name after 'let'");
        AstPtr hint = type_hint();
        expect(Tok::Assign, "'=' after the name");
        auto n = make(Node::Let, kw, name.text);
        n->kids.push_back(expression());
        if (hint) n->kids.push_back(std::move(hint));
        end_of_line();
        return n;
    }

    AstPtr fun_stmt() {
        const Token& kw = advance();
        const Token& name = expect(Tok::Name, "a function name after 'fun'");
        auto n = make(Node::Fun, kw, name.text);
        const Token& open = expect(Tok::LParen, "'(' after the function name");
        auto params = make(Node::Params, open);
        if (!at(Tok::RParen)) {
            do {
                const Token& p = expect(Tok::Name, "a parameter name");
                for (const auto& seen : params->kids)
                    if (seen->text == p.text) fail(p, "duplicate parameter '" + p.text + "'");
                params->kids.push_back(make(Node::Name, p, p.text));
                if (auto hint = type_hint()) params->kids.back()->kids.push_back(std::move(hint));
            } while (match(Tok::Comma));
        }
        expect(Tok::RParen, "')' after the parameters");
        n->kids.push_back(std::move(params));
        AstPtr hint = type_hint();
        if (match(Tok::Assign)) {  // short form: fun double(x) = x * 2
            n->kids.push_back(expression());
            end_of_line();
        } else {
            n->kids.push_back(block());
        }
        if (hint) n->kids.push_back(std::move(hint));
        return n;
    }

    // Optional `as type`. Gives null when there is no `as`.
    AstPtr type_hint() {
        if (!at(Tok::As)) return nullptr;
        advance();
        const Token& t = peek();
        if (t.kind != Tok::Name && t.kind != Tok::None) fail_expected("a type name after 'as'");
        advance();
        static const char* known[] = {"any", "none", "bool", "int", "float", "number", "string",
                                      "list", "map", "function", "module"};
        for (const char* k : known)
            if (t.text == k) return make(Node::Type, t, t.text);
        fail(t, "unknown type '" + t.text + "'; use any, none, bool, int, float, number, string, list, map, function or module");
    }

    AstPtr if_stmt() {
        const Token& kw = advance();
        auto n = make(Node::If, kw);
        n->kids.push_back(expression());
        n->kids.push_back(block());
        if (at(Tok::Else)) {
            const Token& el = advance();
            if (at(Tok::If)) {
                n->kids.push_back(if_stmt());
            } else {
                (void)el;
                n->kids.push_back(block());
            }
        }
        return n;
    }

    AstPtr while_stmt() {
        const Token& kw = advance();
        auto n = make(Node::While, kw);
        n->kids.push_back(expression());
        n->kids.push_back(block());
        return n;
    }

    AstPtr for_stmt() {
        const Token& kw = advance();
        const Token& name = expect(Tok::Name, "a loop name after 'for'");
        expect(Tok::In, "'in' after the loop name");
        auto n = make(Node::For, kw, name.text);
        n->kids.push_back(expression());
        n->kids.push_back(block());
        return n;
    }

    AstPtr expr_stmt() {
        const Token& start = peek();
        auto e = expression();
        if (at(Tok::Assign)) {
            const Token& eq = advance();
            if (e->kind != Node::Name && e->kind != Node::Index && e->kind != Node::Field)
                fail(eq, "can only assign to a name, an index or a field");
            auto n = make(Node::Assign, start);
            n->kids.push_back(std::move(e));
            n->kids.push_back(expression());
            end_of_line();
            return n;
        }
        auto n = make(Node::ExprStmt, start);
        n->kids.push_back(std::move(e));
        end_of_line();
        return n;
    }

    // ---- expressions, lowest precedence first ----

    AstPtr expression() {
        Nest nest(*this);
        return or_expr();
    }

    AstPtr binary(AstPtr left, const Token& op, AstPtr right) {
        auto n = make(Node::Binary, *left, op.text);
        n->line = op.line;
        n->col = op.col;
        n->kids.push_back(std::move(left));
        n->kids.push_back(std::move(right));
        return n;
    }

    AstPtr or_expr() {
        auto left = and_expr();
        while (at(Tok::Or)) {
            const Token& op = advance();
            left = binary(std::move(left), op, and_expr());
        }
        return left;
    }

    AstPtr and_expr() {
        auto left = not_expr();
        while (at(Tok::And)) {
            const Token& op = advance();
            left = binary(std::move(left), op, not_expr());
        }
        return left;
    }

    AstPtr not_expr() {
        if (at(Tok::Not)) {
            const Token& op = advance();
            auto n = make(Node::Unary, op, "not");
            Nest nest(*this);
            n->kids.push_back(not_expr());
            return n;
        }
        return compare();
    }

    static bool is_compare(Tok k) {
        return k == Tok::EqEq || k == Tok::BangEq || k == Tok::Less || k == Tok::LessEq ||
               k == Tok::Greater || k == Tok::GreaterEq || k == Tok::In;
    }

    // Comparisons do not chain: `a < b < c` is an error.
    AstPtr compare() {
        auto left = range();
        if (is_compare(peek().kind)) {
            const Token& op = advance();
            left = binary(std::move(left), op, range());
            if (is_compare(peek().kind))
                fail(peek(), "comparisons cannot be chained; use 'and'");
        }
        return left;
    }

    AstPtr range() {
        auto left = additive();
        if (at(Tok::DotDot)) {
            const Token& op = advance();
            auto n = make(Node::Range, op);
            n->kids.push_back(std::move(left));
            n->kids.push_back(additive());
            return n;
        }
        return left;
    }

    AstPtr additive() {
        auto left = multiplicative();
        while (at(Tok::Plus) || at(Tok::Minus)) {
            const Token& op = advance();
            left = binary(std::move(left), op, multiplicative());
        }
        return left;
    }

    AstPtr multiplicative() {
        auto left = unary();
        while (at(Tok::Star) || at(Tok::Slash) || at(Tok::Percent)) {
            const Token& op = advance();
            left = binary(std::move(left), op, unary());
        }
        return left;
    }

    AstPtr unary() {
        if (at(Tok::Minus)) {
            const Token& op = advance();
            auto n = make(Node::Unary, op, "-");
            Nest nest(*this);
            n->kids.push_back(unary());
            return n;
        }
        return postfix();
    }

    AstPtr postfix() {
        auto e = primary();
        while (true) {
            if (at(Tok::LParen)) {
                const Token& open = advance();
                auto call = make(Node::Call, open);
                call->kids.push_back(std::move(e));
                if (!at(Tok::RParen)) {
                    do call->kids.push_back(expression());
                    while (match(Tok::Comma) && !at(Tok::RParen));
                }
                expect(Tok::RParen, "')' after the arguments");
                e = std::move(call);
            } else if (at(Tok::LBracket)) {
                const Token& open = advance();
                auto idx = make(Node::Index, open);
                idx->kids.push_back(std::move(e));
                idx->kids.push_back(expression());
                expect(Tok::RBracket, "']' after the index");
                e = std::move(idx);
            } else if (at(Tok::Dot)) {
                const Token& dot = advance();
                const Token& name = expect(Tok::Name, "a field name after '.'");
                auto f = make(Node::Field, dot, name.text);
                f->kids.push_back(std::move(e));
                e = std::move(f);
            } else {
                return e;
            }
        }
    }

    AstPtr primary() {
        const Token& t = peek();
        switch (t.kind) {
            case Tok::Number: advance(); return make(Node::Number, t, t.text);
            case Tok::String: advance(); return make(Node::String, t, t.text);
            case Tok::StringPart: return interp();
            case Tok::Name: advance(); return make(Node::Name, t, t.text);
            case Tok::True: advance(); return make(Node::True, t);
            case Tok::False: advance(); return make(Node::False, t);
            case Tok::None: advance(); return make(Node::None, t);
            case Tok::LParen: {
                advance();
                auto e = expression();
                expect(Tok::RParen, "')'");
                return e;
            }
            case Tok::LBracket: {
                advance();
                auto list = make(Node::List, t);
                if (!at(Tok::RBracket)) {
                    do list->kids.push_back(expression());
                    while (match(Tok::Comma) && !at(Tok::RBracket));
                }
                expect(Tok::RBracket, "']' to close the list");
                return list;
            }
            case Tok::LBrace: return map_literal();
            default: fail_expected("an expression");
        }
    }

    AstPtr map_literal() {
        const Token& open = advance();
        auto map = make(Node::Map, open);
        if (!at(Tok::RBrace)) {
            do {
                const Token& key = peek();
                if (key.kind != Tok::Name && key.kind != Tok::String)
                    fail_expected("a map key (a name or a string)");
                advance();
                for (const auto& seen : map->kids)
                    if (seen->text == key.text) fail(key, "duplicate map key '" + key.text + "'");
                expect(Tok::Colon, "':' after the map key");
                auto pair = make(Node::Pair, key, key.text);
                pair->kids.push_back(expression());
                map->kids.push_back(std::move(pair));
            } while (match(Tok::Comma) && !at(Tok::RBrace));
        }
        expect(Tok::RBrace, "'}' to close the map");
        return map;
    }

    // STRING_PART INTERP_START expr INTERP_END (STRING_PART INTERP_START expr INTERP_END)* STRING_END
    AstPtr interp() {
        const Token& first = peek();
        auto n = make(Node::Interp, first);
        while (true) {
            const Token& part = advance();  // StringPart or StringEnd
            if (!part.text.empty()) n->kids.push_back(make(Node::String, part, part.text));
            if (part.kind == Tok::StringEnd) return n;
            expect(Tok::InterpStart, "'{'");
            if (at(Tok::InterpEnd)) fail(peek(), "empty '{}' in a string; use '{{' for a brace");
            n->kids.push_back(expression());
            expect(Tok::InterpEnd, "'}' to close the value in the string");
            if (!at(Tok::StringPart) && !at(Tok::StringEnd)) fail_expected("the rest of the string");
        }
    }
};

}  // namespace

AstPtr parse(const std::vector<Token>& tokens) { return Parser(tokens).program(); }

}  // namespace relic
