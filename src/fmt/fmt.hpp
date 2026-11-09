#pragma once
#include <string>

namespace relic {

// Result of formatting one file. On success `out` is the canonical source
// (LF, one final newline when the file is not empty) and `exit_code` is 0.
// A lex or parse error leaves `out` empty, sets `exit_code` to 1, and puts
// the usual `path:line:col: error:` text in `err`.
struct FmtResult {
    int exit_code = 0;
    std::string out;
    std::string err;
};

// Canonical formatting of one Relic source. Does not touch the language:
// a file the lexer or parser rejects is not rewritten. Comments stay, in
// place, including comments the parser drops. Formatting again changes nothing.
FmtResult format_source(const std::string& path, const std::string& source);

}  // namespace relic
