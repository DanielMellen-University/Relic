#pragma once
#include <string>

namespace relic::diag {

// Formats an error as "path:line:col: error: message".
std::string format(const std::string& path, int line, int col, const std::string& msg);

}  // namespace relic::diag
