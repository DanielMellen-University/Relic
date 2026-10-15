// Small hand-written test harness (see docs/TESTING.md). No test library.
#include "core.hpp"
#include "diag/diag.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <iostream>
#include <string>
#include <vector>

static int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << __FILE__ << ":" << __LINE__ << ": FAIL " #cond "\n"; \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

using relic::Tok;

static std::vector<Tok> kinds(const std::string& src) {
    std::vector<Tok> k;
    for (const auto& t : relic::lex(src)) k.push_back(t.kind);
    return k;
}

// Returns "line:col message" for a lex error, or "" if lexing succeeded.
static std::string lex_error(const std::string& src) {
    try {
        relic::lex(src);
    } catch (const relic::LexError& e) {
        return std::to_string(e.line) + ":" + std::to_string(e.col) + " " + e.what();
    }
    return "";
}

static void test_core() {
    CHECK(relic::version_string().rfind("relic ", 0) == 0);
    CHECK(relic::diag::format("a.relic", 3, 5, "x") == "a.relic:3:5: error: x\n");
    auto empty = relic::run_source("e.relic", "");
    CHECK(empty.exit_code == 0 && empty.out.empty());
    auto comments = relic::run_source("c.relic", "# hi\n\n    # indented\n");
    CHECK(comments.exit_code == 0 && comments.err.empty());
    auto code = relic::run_source("p.relic", "\nprint(\"hi\")\n");
    CHECK(code.exit_code == 1);
    CHECK(code.err.rfind("p.relic:2:1: error:", 0) == 0);
}

static void test_basic_tokens() {
    CHECK((kinds("") == std::vector<Tok>{Tok::Eof}));
    CHECK((kinds("let x = 1") ==
           std::vector<Tok>{Tok::Let, Tok::Name, Tok::Assign, Tok::Number, Tok::Newline, Tok::Eof}));
    CHECK((kinds("a == b != c <= d >= e < f > g") ==
           std::vector<Tok>{Tok::Name, Tok::EqEq, Tok::Name, Tok::BangEq, Tok::Name, Tok::LessEq,
                            Tok::Name, Tok::GreaterEq, Tok::Name, Tok::Less, Tok::Name,
                            Tok::Greater, Tok::Name, Tok::Newline, Tok::Eof}));
    CHECK((kinds("fun if else while for in break next return and or not true false none") ==
           std::vector<Tok>{Tok::Fun, Tok::If, Tok::Else, Tok::While, Tok::For, Tok::In,
                            Tok::Break, Tok::Next, Tok::Return, Tok::And, Tok::Or, Tok::Not,
                            Tok::True, Tok::False, Tok::None, Tok::Newline, Tok::Eof}));
    CHECK(relic::lex("funny")[0].kind == Tok::Name);
}

static void test_numbers() {
    auto t = relic::lex("3.25 0..10 x.y");
    CHECK(t[0].kind == Tok::Number && t[0].text == "3.25");
    CHECK(t[1].kind == Tok::Number && t[1].text == "0");
    CHECK(t[2].kind == Tok::DotDot);
    CHECK(t[3].kind == Tok::Number && t[3].text == "10");
    CHECK(t[5].kind == Tok::Dot);
    CHECK(lex_error("12abc") == "1:3 letter right after a number");
}

static void test_strings() {
    auto t = relic::lex("\"a\\n\\t\\\"\\\\b\"");
    CHECK(t[0].kind == Tok::String && t[0].text == "a\n\t\"\\b");
    CHECK(relic::lex("\"{{x}}\"")[0].text == "{x}");
    CHECK((kinds("\"hi {name}!\"") ==
           std::vector<Tok>{Tok::StringPart, Tok::InterpStart, Tok::Name, Tok::InterpEnd,
                            Tok::StringEnd, Tok::Newline, Tok::Eof}));
    // map literal inside an interpolation
    CHECK((kinds("\"{ {a: 1}.a }\"") ==
           std::vector<Tok>{Tok::StringPart, Tok::InterpStart, Tok::LBrace, Tok::Name, Tok::Colon,
                            Tok::Number, Tok::RBrace, Tok::Dot, Tok::Name, Tok::InterpEnd,
                            Tok::StringEnd, Tok::Newline, Tok::Eof}));
    CHECK(lex_error("\"open") == "1:1 unterminated string");
    CHECK(lex_error("\"a {b") == "1:1 unterminated string");
    CHECK(lex_error("\"\\q\"") == "1:2 unknown escape '\\q'");
}

static void test_layout() {
    CHECK((kinds("if a\n    b\nc") ==
           std::vector<Tok>{Tok::If, Tok::Name, Tok::Newline, Tok::Indent, Tok::Name, Tok::Newline,
                            Tok::Dedent, Tok::Name, Tok::Newline, Tok::Eof}));
    // blank and comment lines do not affect layout; EOF closes open blocks
    CHECK((kinds("if a\n\n    # note\n    b\n") ==
           std::vector<Tok>{Tok::If, Tok::Name, Tok::Newline, Tok::Indent, Tok::Name, Tok::Newline,
                            Tok::Dedent, Tok::Eof}));
    // newlines inside brackets join lines
    CHECK((kinds("f(1,\n        2)") ==
           std::vector<Tok>{Tok::Name, Tok::LParen, Tok::Number, Tok::Comma, Tok::Number,
                            Tok::RParen, Tok::Newline, Tok::Eof}));
    CHECK(kinds("a\r\nb\r\n").size() == 5);
    CHECK(lex_error("if a\n\tb") == "2:1 tabs are not allowed; use 4 spaces");
    CHECK(lex_error("if a\n  b") ==
          "2:3 indent must be exactly 4 spaces deeper than the line above");
    CHECK(lex_error("if a\n    if b\n        c\n  d") ==
          "4:3 dedent does not line up with any outer block");
    CHECK(lex_error("    a") == "1:5 unexpected indent");
    CHECK(lex_error("f(1]") == "1:4 unmatched ']'");
    CHECK(lex_error("f(1") == "1:2 unclosed '('");
    CHECK(lex_error("a ! b") == "1:3 unexpected '!'; use 'not' for logic");
}

// Parses and returns the tree dump with the "Program" line removed.
static std::string tree(const std::string& src) {
    std::string out = relic::dump_ast(*relic::parse(relic::lex(src)));
    return out.substr(out.find('\n') + 1);
}

static std::string parse_error(const std::string& src) {
    try {
        relic::parse(relic::lex(src));
    } catch (const relic::ParseError& e) {
        return std::to_string(e.line) + ":" + std::to_string(e.col) + " " + e.what();
    }
    return "";
}

static void test_parser() {
    // precedence: * before +, + before comparison, comparison before and/or
    CHECK(tree("1 + 2 * 3") ==
          "  ExprStmt\n    Binary +\n      Number 1\n      Binary *\n        Number 2\n"
          "        Number 3\n");
    CHECK(tree("a or b and c") ==
          "  ExprStmt\n    Binary or\n      Name a\n      Binary and\n        Name b\n"
          "        Name c\n");
    CHECK(tree("not a == b") ==
          "  ExprStmt\n    Unary not\n      Binary ==\n        Name a\n        Name b\n");
    CHECK(tree("1 - 2 - 3") ==
          "  ExprStmt\n    Binary -\n      Binary -\n        Number 1\n        Number 2\n"
          "      Number 3\n");
    CHECK(tree("(1 + 2) * 3").find("Binary *\n      Binary +") != std::string::npos);
    CHECK(tree("a.b[0](x)") ==
          "  ExprStmt\n    Call\n      Index\n        Field b\n          Name a\n"
          "        Number 0\n      Name x\n");
    CHECK(tree("0..n + 1") ==
          "  ExprStmt\n    Range\n      Number 0\n      Binary +\n        Name n\n"
          "        Number 1\n");
    // trailing commas and wrapped literals
    CHECK(tree("[1,\n 2,\n]") == "  ExprStmt\n    List\n      Number 1\n      Number 2\n");
    CHECK(tree("{}") == "  ExprStmt\n    Map\n");
    CHECK(tree("f()") == "  ExprStmt\n    Call\n      Name f\n");
    // statements
    CHECK(tree("fun f() = 1") == "  Fun f\n    Params\n    Number 1\n");
    CHECK(tree("if a\n    b\nelse if c\n    d\nelse\n    e") ==
          "  If\n    Name a\n    Block\n      ExprStmt\n        Name b\n    If\n      Name c\n"
          "      Block\n        ExprStmt\n          Name d\n      Block\n        ExprStmt\n"
          "          Name e\n");
    CHECK(tree("return") == "  Return\n");
    CHECK(tree("\"a{x}{y}b\"") ==
          "  ExprStmt\n    Interp\n      String \"a\"\n      Name x\n      Name y\n"
          "      String \"b\"\n");
    // errors
    CHECK(parse_error("if a\nb") == "2:1 expected an indented block, got name 'b'");
    CHECK(parse_error("if a") == "1:5 expected an indented block, got end of file");
    CHECK(parse_error("else\n    a") == "1:1 'else' without a matching 'if'");
    CHECK(parse_error("a < b < c") == "1:7 comparisons cannot be chained; use 'and'");
    CHECK(parse_error("1 = 2") == "1:3 can only assign to a name, an index or a field");
    CHECK(parse_error("f(1 2)") == "1:5 expected ')' after the arguments, got number 2");
    CHECK(parse_error("let = 1") == "1:5 expected a name after 'let', got '='");
    CHECK(parse_error("let x = ") == "1:9 expected an expression, got end of line");
    CHECK(parse_error("{1: 2}") == "1:2 expected a map key (a name or a string), got number 1");
    CHECK(parse_error("fun f(a, a)\n    a") == "1:10 duplicate parameter 'a'");
    CHECK(parse_error("for 1 in x\n    a") == "1:5 expected a loop name after 'for', got number 1");
    CHECK(parse_error("for i x\n    a") == "1:7 expected 'in' after the loop name, got name 'x'");
}

int main() {
    test_parser();
    test_core();
    test_basic_tokens();
    test_numbers();
    test_strings();
    test_layout();
    if (failures) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "unit: all passed\n";
    return 0;
}
