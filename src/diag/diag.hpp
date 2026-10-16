#pragma once
#include <string>

namespace relic::diag {

// Formats an error as "path:line:col: error: message", then the source
// line with a caret under the column when the file can be read.
std::string format(const std::string& path, int line, int col, const std::string& msg,
                   const char* kind = "error");

}  // namespace relic::diag
