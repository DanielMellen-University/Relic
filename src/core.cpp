#include "core.hpp"
#include "diag/diag.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "interp/interp.hpp"

#include <sstream>
#include "version.hpp"

namespace relic {

std::string version_string() { return std::string("relic ") + RELIC_VERSION; }

namespace {

std::string quote(const std::string& s) {
    std::string q = "\"";
    for (char c : s) {
        if (c == '\n') q += "\\n";
        else if (c == '\t') q += "\\t";
        else if (c == '"') q += "\\\"";
        else if (c == '\\') q += "\\\\";
        else q += c;
    }
    return q + "\"";
}

bool has_text(Tok k) {
    switch (k) {
        case Tok::Newline: case Tok::Indent: case Tok::Dedent: case Tok::Eof: return false;
        default: return true;
    }
}

bool is_string(Tok k) { return k == Tok::String || k == Tok::StringPart || k == Tok::StringEnd; }

}  // namespace

RunResult dump_tokens(const std::string& path, const std::string& source) {
    RunResult r;
    try {
        for (const Token& t : lex(source)) {
            r.out += std::to_string(t.line) + ":" + std::to_string(t.col) + " " + tok_name(t.kind);
            if (has_text(t.kind)) r.out += " " + (is_string(t.kind) ? quote(t.text) : t.text);
            r.out += "\n";
        }
    } catch (const LexError& e) {
        r.exit_code = 1;
        r.out.clear();
        r.err = diag::format(path, e.line, e.col, e.what());
    }
    return r;
}

RunResult dump_tree(const std::string& path, const std::string& source) {
    RunResult r;
    try {
        r.out = dump_ast(*parse(lex(source)));
    } catch (const LexError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
    } catch (const ParseError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
    }
    return r;
}

RunResult run_source(const std::string& path, const std::string& source) {
    RunResult r;
    try {
        auto prog = parse(lex(source));
        std::ostringstream out;
        Interpreter interp(out);
        try {
            interp.run(*prog);
        } catch (const RuntimeError& e) {
            r.exit_code = 1;
            r.err = diag::format(path, e.line, e.col, e.what());
        }
        r.out = out.str();
    } catch (const LexError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
    } catch (const ParseError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
    }
    return r;
}

}  // namespace relic
