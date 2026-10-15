#include "diag/diag.hpp"

namespace relic::diag {

std::string format(const std::string& path, int line, int col, const std::string& msg) {
    return path + ":" + std::to_string(line) + ":" + std::to_string(col) + ": error: " + msg + "\n";
}

}  // namespace relic::diag
