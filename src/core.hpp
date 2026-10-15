#pragma once
#include <string>

namespace relic {

// Shared entry point for the file runner (and later the REPL).
// M1 stub: accepts blank or comment-only source and produces no output.
// Anything else reports "not implemented yet". Real pipeline lands in M2 to M4.
struct RunResult {
    int exit_code = 0;
    std::string out;
    std::string err;
};

RunResult run_source(const std::string& path, const std::string& source);

std::string version_string();

}  // namespace relic
