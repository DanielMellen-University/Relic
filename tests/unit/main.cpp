// Small hand-written test harness (see docs/TESTING.md). No test library.
#include "core.hpp"
#include "diag/diag.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

// The built-in 8x12 text font (std/graphics text), shared with runtime/c.
#include "builtins/font8x12.inc"

#include <iostream>
#include <sstream>
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

// Every printable glyph but space draws something. '_' used to be blank
// (DejaVu puts it below the cell); it is a bar just under the baseline.
static void test_font() {
    for (int c = 33; c < 127; c++) {
        int bits = 0;
        for (int row = 0; row < 12; row++) bits |= kFont8x12[c - 32][row];
        if (bits == 0) std::cerr << "blank glyph '" << static_cast<char>(c) << "'\n";
        CHECK(bits != 0);
    }
    for (int row = 0; row < 12; row++) CHECK(kFont8x12[0][row] == 0);
    const unsigned char* u = kFont8x12['_' - 32];
    CHECK(u[10] == 0x7e);
    for (int row = 0; row < 10; row++) CHECK(u[row] == 0);
}

static void test_core() {
    CHECK(relic::version_string().rfind("relic ", 0) == 0);
    CHECK(relic::diag::format("a.relic", 3, 5, "x") == "a.relic:3:5: error: x\n");
    auto empty = relic::run_source("e.relic", "");
    CHECK(empty.exit_code == 0 && empty.out.empty());
    auto comments = relic::run_source("c.relic", "# hi\n\n    # indented\n");
    CHECK(comments.exit_code == 0 && comments.err.empty());
    auto code = relic::run_source("p.relic", "\nprint(\"hi\")\n");
    CHECK(code.exit_code == 0 && code.out == "hi\n");
    auto bad = relic::run_source("p.relic", "print(1)\nprint(x)\n");
    CHECK(bad.exit_code == 1 && bad.out == "1\n");
    CHECK(bad.err == "p.relic:2:7: error: 'x' is not defined\n");
    // Hostile nesting is an error, not a stack overflow.
    std::string deep = "print(" + std::string(20000, '(') + "1" + std::string(20000, ')') + ")\n";
    std::string negs = "let x = " + std::string(20000, '-') + "1\n";
    for (const std::string& src : {deep, negs}) {
        auto a = relic::run_source("n.relic", src);
        auto b = relic::run_source_vm("n.relic", src);
        auto c = relic::check_source("n.relic", src);
        CHECK(a.exit_code == 1 && a.err.find("too deeply nested") != std::string::npos);
        CHECK(b.err == a.err && c.err == a.err);
    }
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
    // characters that look like something else, or like nothing, say what they are
    CHECK(lex_error("\xEF\xBB\xBFprint(1)") ==
          "1:1 unexpected character U+FEFF (a byte order mark; save the file as UTF-8 without one)");
    CHECK(lex_error("let a\xC2\xA0= 1") ==
          "1:6 unexpected character U+00A0 (a no-break space; use a plain space)");
    CHECK(lex_error("a\xE2\x80\x8B") == "1:2 unexpected character U+200B (a zero-width space; delete it)");
    CHECK(lex_error("a\xE3\x80\x80" "b") ==
          "1:2 unexpected character U+3000 (an ideographic space; use a plain space)");
    CHECK(lex_error("print(\xE2\x80\x9Chi\xE2\x80\x9D)") ==
          "1:7 unexpected character '\xE2\x80\x9C' (a curly double quote; use a straight one)");
    CHECK(lex_error("\xE2\x80\x98x\xE2\x80\x99") ==
          "1:1 unexpected character '\xE2\x80\x98' (a curly single quote; strings use straight double quotes)");
    CHECK(lex_error("a $ b") == "1:3 unexpected character '$'");
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

// Parse errors for each kind of statement and expression: where the caret goes
// and what the message says (the unclosed-bracket cases come from the lexer, so
// they are in test_layout).
static void test_parse_errors() {
    CHECK(parse_error("use\n") == "1:4 expected a module name after 'use', got end of line");
    CHECK(parse_error("use 5\n") == "1:5 expected a module name after 'use', got number 5");
    CHECK(parse_error("use a as\n") == "1:9 expected a name after 'as', got end of line");
    CHECK(parse_error("fun\n") == "1:4 expected a function name after 'fun', got end of line");
    CHECK(parse_error("fun f\n") == "1:6 expected '(' after the function name, got end of line");
    CHECK(parse_error("fun f(a b)\n    a\n") == "1:9 expected ')' after the parameters, got name 'b'");
    CHECK(parse_error("fun f(a as)\n    a\n") == "1:11 expected a type name after 'as', got ')'");
    CHECK(parse_error("fun f() as\n    1\n") == "1:11 expected a type name after 'as', got end of line");
    CHECK(parse_error("let\n") == "1:4 expected a name after 'let', got end of line");
    CHECK(parse_error("let x as\n") == "1:9 expected a type name after 'as', got end of line");
    CHECK(parse_error("let x as int\n") == "1:13 expected '=' after the name, got end of line");
    CHECK(parse_error("try\n    1\n") == "3:1 expected 'catch' after the 'try' block, got end of file");
    CHECK(parse_error("try\n    1\ncatch 5\n    2\n") == "3:7 expected end of line before the block, got number 5");
    CHECK(parse_error("while\n    1\n") == "1:6 expected an expression, got end of line");
    CHECK(parse_error("for\n    1\n") == "1:4 expected a loop name after 'for', got end of line");
    CHECK(parse_error("for i in\n    1\n") == "1:9 expected an expression, got end of line");
    CHECK(parse_error("break 5\n") == "1:7 expected end of line, got number 5");
    CHECK(parse_error("return 1 2\n") == "1:10 expected end of line, got number 2");
    CHECK(parse_error("x.\n") == "1:3 expected a field name after '.', got end of line");
    CHECK(parse_error("x.5\n") == "1:3 expected a field name after '.', got number 5");
    CHECK(parse_error("[1 2]\n") == "1:4 expected ']' to close the list, got number 2");
    CHECK(parse_error("{a 1}\n") == "1:4 expected ':' after the map key, got number 1");
    CHECK(parse_error("{a: 1 b: 2}\n") == "1:7 expected '}' to close the map, got name 'b'");
    CHECK(parse_error("{\"a\"}\n") == "1:5 expected ':' after the map key, got '}'");
    CHECK(parse_error("()\n") == "1:2 expected an expression, got ')'");
    CHECK(parse_error("1..\n") == "1:4 expected an expression, got end of line");
    CHECK(parse_error("f(,)\n") == "1:3 expected an expression, got ','");
    CHECK(parse_error("x = = 1\n") == "1:5 expected an expression, got '='");
    CHECK(parse_error("fun f() = \n") == "1:11 expected an expression, got end of line");
    CHECK(parse_error("catch e\n    1\n") == "1:1 'catch' without a matching 'try'");
    // 'catch' may leave out the name
    CHECK(parse_error("try\n    1\ncatch\n    2\n").empty());
}

// Runs a program and returns stdout, or "error: ..." on failure.
static std::string run(const std::string& src) {
    auto r = relic::run_source("t.relic", src);
    if (r.exit_code != 0) return "error: " + r.err.substr(r.err.find("error: ") + 7);
    return r.out;
}

static void test_interp() {
    CHECK(run("print(2 + 3 * 4)") == "14\n");
    CHECK(run("print(10 / 4, 9 % 4, -1 % 4)") == "2.5 1 3\n");
    CHECK(run("print(\"a\" == \"a\", [1] != [2], {a: 1} == {a: 1})") == "true true true\n");
    CHECK(run("let x = 1\nx = x + 1\nprint(x)") == "2\n");
    CHECK(run("fun f(n) = n * 2\nprint(f(4))") == "8\n");
    CHECK(run("let xs = [3]\nlet ys = xs\nappend(ys, 4)\nprint(xs)") == "[3, 4]\n");
    CHECK(run("for i in 0..3\n    print(i)") == "0\n1\n2\n");
    CHECK(run("print(false and nope)") == "false\n");  // short circuit
    CHECK(run("print(true or nope)") == "true\n");
    CHECK(run("print(\"{1 + 1}x\")") == "2x\n");
    CHECK(run("print(len([1, 2]), len({}), len(\"\"))") == "2 0 0\n");
    CHECK(run("print(1 and true)") ==
          "error: 'and' operand must be true or false, got int\n");
    CHECK(run("print(\"a\" * 2)") == "error: cannot multiply a string and an int\n");
    CHECK(run("let xs = [1]\nprint(xs[0.5])") == "error: list index must be a whole number\n");
    CHECK(run("next") == "error: 'next' outside a loop\n");
    CHECK(run("len(1, 2)") == "error: len() takes 1 argument, got 2\n");
    CHECK(run("append(1, 2)") == "error: append(): first argument must be a list, got int\n");
}

// Built-in error messages and edge results (the host's wording is the spec the
// self-hosted interpreter and compiled programs copy).
static void test_builtins() {
    CHECK(run("insert([1], 5, 9)") == "error: insert(): index 5 is out of range\n");
    CHECK(run("insert([1], \"a\", 9)") == "error: insert(): index must be a number, got string\n");
    CHECK(run("remove([1], 7)") == "error: remove(): index 7 is out of range\n");
    CHECK(run("remove({a: 1}, \"b\")") == "error: remove(): map has no key 'b'\n");
    CHECK(run("pop([])") == "error: pop(): list is empty\n");
    CHECK(run("ord(\"\")") == "error: ord(): needs a one-character string\n");
    CHECK(run("ord(\"ab\")") == "error: ord(): needs a one-character string\n");
    CHECK(run("chr(-1)") ==
          "error: chr(): code must be 0 to 1114111 and not 55296 to 57343 (surrogates)\n");
    CHECK(run("chr(55296)") ==
          "error: chr(): code must be 0 to 1114111 and not 55296 to 57343 (surrogates)\n");
    CHECK(run("chr(\"a\")") == "error: chr(): argument must be a number, got string\n");
    CHECK(run("int(\"x\")") == "error: int(): not a number: \"x\"\n");
    CHECK(run("int([])") == "error: int(): cannot turn a list into an int\n");
    CHECK(run("float(none)") == "error: float(): cannot turn a none into a float\n");
    CHECK(run("keys([1])") == "error: keys(): argument must be a map, got list\n");
    CHECK(run("len(none)") == "error: len(): cannot get the length of a none\n");
    CHECK(run("str()") == "error: str() takes 1 argument, got 0\n");
    CHECK(run("args(1)") == "error: args() takes 0 arguments, got 1\n");
    CHECK(run("append([1])") == "error: append() takes 2 arguments, got 1\n");
    CHECK(run("fail(5)") == "error: fail(): message must be a string, got int\n");
    CHECK(run("fail(\"boom\")") == "error: boom\n");
    // results: slice clamps, pop / remove / insert change the list in place
    CHECK(run("print(slice([1, 2, 3], 2, 9), slice(\"abc\", -1, 2), slice([1], 1, 0))") ==
          "[3] ab []\n");
    CHECK(run("let xs = [1, 2, 3]\nprint(pop(xs), remove(xs, 0))\nprint(xs)") == "3 1\n[2]\n");
    CHECK(run("let xs = [2]\ninsert(xs, 0, 1)\ninsert(xs, 2, 3)\nprint(xs)") == "[1, 2, 3]\n");
    CHECK(run("print(ord(\"A\"), chr(955), ord(chr(955)), len(chr(955)))") == "65 λ 955 1\n");
    CHECK(run("print(int(3.9), int(-3.9), float(2), int(\"42\"), float(\"2.5\"))") ==
          "3 -3 2.0 42 2.5\n");
    CHECK(run("print(str(1.0), str(-0.0), str([1, \"a\"]), str(none), str(true))") ==
          "1.0 -0.0 [1, \"a\"] none true\n");
    CHECK(run("print(keys({b: 1, a: 2}))") == "[\"b\", \"a\"]\n");  // insertion order
}

static void test_repl() {
    std::istringstream in("let x = 1\nx = x + 1\nnope\nx\nfun f()\n    return x * 10\n\nf()\n[1,\n2]\n");
    std::ostringstream out, err;
    CHECK(relic::repl(in, out, err, false) == 0);
    CHECK(out.str() == "2\n20\n[1, 2]\n");
    CHECK(err.str() == "<repl>:1:1: error: 'nope' is not defined\n");

    // a session keeps globals across runs, like the REPL does
    std::ostringstream o2;
    relic::Session s(o2);
    CHECK(s.run("a", "let n = 5").exit_code == 0);
    CHECK(s.run("a", "fun g() = n + 1").exit_code == 0);
    CHECK(s.run("a", "print(g())").exit_code == 0);
    CHECK(o2.str() == "6\n");
}

// REPL details from docs/CLI.md: what a bare expression prints, how errors read
// (line numbers count from the start of that input), :quit, blank input.
static void test_repl_details() {
    auto repl = [](const std::string& input, std::string& out, std::string& err) {
        std::istringstream in(input);
        std::ostringstream o, e;
        int rc = relic::repl(in, o, e, false);
        out = o.str();
        err = e.str();
        return rc;
    };
    std::string out, err;
    CHECK(repl("none\n1 + 1\n\"s\"\n[1, \"a\"]\n{a: 1}\n", out, err) == 0);
    CHECK(out == "2\n\"s\"\n[1, \"a\"]\n{a: 1}\n" && err.empty());
    // errors keep the session going; a block's error line counts inside the block
    CHECK(repl("let a = 1\nlet b =\n\nfun g()\n    return nope\n\ng()\nlen(1, 2)\na\n", out, err) == 0);
    CHECK(out == "1\n");
    CHECK(err == "<repl>:1:8: error: expected an expression, got end of line\n"
                 "<repl>:2:12: error: 'nope' is not defined\n"
                 "<repl>:1:4: error: len() takes 1 argument, got 2\n");
    // :quit stops reading; blank lines and comments run nothing
    CHECK(repl("\n  \n# note\n5\n:quit\n6\n", out, err) == 0);
    CHECK(out == "5\n" && err.empty());
    // an open bracket keeps reading, a try block runs as one chunk
    CHECK(repl("[1,\n2]\ntry\n    fail(\"x\")\ncatch e\n    print(e.message)\n\n", out, err) == 0);
    CHECK(out == "[1, 2]\nx\n" && err.empty());
    CHECK(repl("let l = [1]\nl[5]\nl\n", out, err) == 0);
    CHECK(out == "[1]\n" && err == "<repl>:1:3: error: list index 5 is out of range (size 1)\n");
}

int main() {
    test_parser();
    test_parse_errors();
    test_repl();
    test_repl_details();
    test_interp();
    test_builtins();
    test_core();
    test_font();
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
