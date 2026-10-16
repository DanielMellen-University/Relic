#include "core.hpp"
#include "diag/diag.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "interp/interp.hpp"
#include "vm/vm.hpp"

#include <istream>
#include <ostream>
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

Session::Session(std::ostream& out) : out_(out), interp_(std::make_unique<Interpreter>(out)) {}
Session::~Session() = default;

RunResult Session::run(const std::string& path, const std::string& source, bool echo) {
    RunResult r;
    try {
        trees_.push_back(parse(lex(source)));
        Value last;
        if (!main_set_) {
            interp_->set_main_file(path);
            main_set_ = true;
        }
        interp_->run(*trees_.back(), echo ? &last : nullptr);
        if (echo && !last.is<NoneV>()) out_ << repr(last) << "\n";
    } catch (const LexError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
    } catch (const ParseError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
    } catch (const RuntimeError& e) {
        r.exit_code = 1;
        r.err = diag::format(e.file.empty() ? path : e.file, e.line, e.col, e.what());
    }
    return r;
}

RunResult run_source_vm(const std::string& path, const std::string& source) {
    std::ostringstream out;
    RunResult r = run_source_vm(path, source, out);
    r.out = out.str();
    return r;
}

RunResult run_source_vm(const std::string& path, const std::string& source, std::ostream& out) {
    RunResult r;
    try {
        auto tree = parse(lex(source));
        vm_run(*tree, out, path);
    } catch (const LexError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
    } catch (const ParseError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
    } catch (const RuntimeError& e) {
        r.exit_code = 1;
        r.err = diag::format(e.file.empty() ? path : e.file, e.line, e.col, e.what());
    }
    return r;
}

RunResult check_source(const std::string& path, const std::string& source) {
    RunResult r;
    try {
        if (vm_check(*parse(lex(source)), path, r.err) > 0) r.exit_code = 1;
    } catch (const LexError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
    } catch (const ParseError& e) {
        r.exit_code = 1;
        r.err = diag::format(path, e.line, e.col, e.what());
    }
    return r;
}

RunResult dump_bytecode(const std::string& path, const std::string& source) {
    RunResult r;
    try {
        r.out = vm_disassemble(*parse(lex(source)));
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
    std::ostringstream out;
    Session session(out);
    RunResult r = session.run(path, source);
    r.out = out.str();
    return r;
}

RunResult run_source(const std::string& path, const std::string& source, std::ostream& out) {
    Session session(out);
    return session.run(path, source);
}

namespace {

// A line opens a block when it starts with a block keyword, except the
// one-line `fun f(x) = expr` form.
bool opens_block(const std::string& line) {
    try {
        auto t = lex(line);
        Tok k = t.front().kind;
        if (k == Tok::If || k == Tok::While || k == Tok::For || k == Tok::Else) return true;
        if (k == Tok::Fun) {
            int depth = 0;
            for (const auto& tok : t) {
                if (tok.kind == Tok::LParen) ++depth;
                if (tok.kind == Tok::RParen) --depth;
                if (tok.kind == Tok::Assign && depth == 0) return false;
            }
            return true;
        }
    } catch (const LexError&) {
    }
    return false;
}

// True when the text has an open bracket that a later line could close.
bool unclosed(const std::string& text) {
    try {
        lex(text);
    } catch (const LexError& e) {
        return std::string(e.what()).rfind("unclosed", 0) == 0;
    }
    return false;
}

}  // namespace

int repl(std::istream& in, std::ostream& out, std::ostream& err, bool interactive) {
    Session session(out);
    if (interactive)
        out << version_string() << ". Blocks end with an empty line. Type :quit or press Ctrl-D to leave.\n";
    std::string chunk;
    bool block = false;
    std::string line;
    while (true) {
        if (interactive) out << (chunk.empty() ? "> " : "... ") << std::flush;
        if (!std::getline(in, line)) break;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (chunk.empty()) {
            if (line == ":quit") break;
            if (line.find_first_not_of(' ') == std::string::npos) continue;
            block = opens_block(line);
        }
        bool blank = line.find_first_not_of(' ') == std::string::npos;
        chunk += line + "\n";
        if (block ? !blank : unclosed(chunk)) continue;
        RunResult r = session.run("<repl>", chunk, true);
        out << std::flush;
        err << r.err << std::flush;
        chunk.clear();
        block = false;
    }
    if (!chunk.empty()) {  // input ended in the middle of a block
        RunResult r = session.run("<repl>", chunk, true);
        err << r.err;
    }
    if (interactive) out << "\n";
    return 0;
}

}  // namespace relic
