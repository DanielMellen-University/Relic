#pragma once
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace relic {

struct RunResult {
    int exit_code = 0;
    std::string out;
    std::string err;
};

class Interpreter;
struct Ast;

// One running program: lex, parse, run. The file runner feeds it a whole
// file once. The REPL feeds it one input at a time, and globals stay alive
// between inputs. Program output goes to `out`; errors come back in
// RunResult::err.
class Session {
public:
    explicit Session(std::ostream& out);
    ~Session();
    // echo: print the value of a final bare expression (REPL style).
    RunResult run(const std::string& path, const std::string& source, bool echo = false);

private:
    std::ostream& out_;
    std::unique_ptr<Interpreter> interp_;
    std::vector<std::unique_ptr<Ast>> trees_;
    bool main_set_ = false;  // functions point into these
};

// File runner. Output printed before a runtime error is kept in `out`.
RunResult run_source(const std::string& path, const std::string& source);
// Same, but program output goes straight to `out` as it is printed (the
// command line passes std::cout). RunResult::out stays empty.
RunResult run_source(const std::string& path, const std::string& source, std::ostream& out);

// Interactive loop used by `relic` with no file. Reads from `in` until end of
// input or `:quit`. Prompts and the banner are shown only when `interactive`.
// Returns the exit code.
int repl(std::istream& in, std::ostream& out, std::ostream& err, bool interactive);

// Runs a file on the bytecode VM (--vm). Same output and errors as run_source.
RunResult run_source_vm(const std::string& path, const std::string& source);
RunResult run_source_vm(const std::string& path, const std::string& source, std::ostream& out);

// Backs --bytecode: the compiled code listing.
RunResult dump_bytecode(const std::string& path, const std::string& source);

// Backs the --tokens flag: one token per line as "line:col KIND text".
RunResult dump_tokens(const std::string& path, const std::string& source);

// Backs the --ast flag: prints the parsed tree.
RunResult dump_tree(const std::string& path, const std::string& source);

std::string version_string();

// Words after the file name; read by the args() built-in.
std::vector<std::string>& script_args();

}  // namespace relic
