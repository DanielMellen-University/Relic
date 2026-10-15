#pragma once
#include <string>

namespace relic {

struct RunResult {
    int exit_code = 0;
    std::string out;
    std::string err;
};

// Shared entry point for the file runner (and later the REPL): lex, parse,
// run. Output printed before a runtime error is kept in `out`.
RunResult run_source(const std::string& path, const std::string& source);

// Backs the --tokens flag: one token per line as "line:col KIND text".
RunResult dump_tokens(const std::string& path, const std::string& source);

// Backs the --ast flag: prints the parsed tree.
RunResult dump_tree(const std::string& path, const std::string& source);

std::string version_string();

}  // namespace relic
