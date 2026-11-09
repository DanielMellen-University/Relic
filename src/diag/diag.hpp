#pragma once
#include <string>

namespace relic::diag {

// Extra placement for a diagnostic. Empty hint and an end column of 0
// leave the text identical to a one-column caret.
struct Note {
    int end_col = 0;    // inclusive; 0 or less than col means a single caret
    std::string hint;   // printed as "hint: ..." after the caret line
};

// Formats an error as "path:line:col: error: message", then the source
// line with a caret under the column when the file can be read.
// A Note with an end column draws that many carets, and a hint adds a line.
std::string format(const std::string& path, int line, int col, const std::string& msg,
                   const char* kind = "error");
std::string format(const std::string& path, int line, int col, const std::string& msg,
                   const Note& note, const char* kind = "error");

}  // namespace relic::diag
