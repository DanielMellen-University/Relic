#pragma once
#include <string>

namespace relic {

struct RunResult {
    int exit_code = 0;
    std::string out;
    std::string err;
};

// Shared entry point for the file runner (and later the REPL).
// M2: lexes the source and reports lex errors. Running code is not
// implemented yet: any program with tokens reports "not implemented yet"
// at its first token. The interpreter lands in M4.
RunResult run_source(const std::string& path, const std::string& source);

// Backs the --tokens flag: one token per line as "line:col KIND text".
RunResult dump_tokens(const std::string& path, const std::string& source);

std::string version_string();

}  // namespace relic
