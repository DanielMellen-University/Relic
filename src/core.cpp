#include "core.hpp"
#include "diag/diag.hpp"
#include "version.hpp"

#include <sstream>

namespace relic {

std::string version_string() { return std::string("relic ") + RELIC_VERSION; }

RunResult run_source(const std::string& path, const std::string& source) {
    RunResult r;
    std::istringstream in(source);
    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        size_t i = line.find_first_not_of(" \t\r");
        if (i == std::string::npos || line[i] == '#') continue;
        r.exit_code = 1;
        r.err = diag::format(path, line_no, static_cast<int>(i) + 1,
                             "not implemented yet: the interpreter arrives in M4");
        return r;
    }
    return r;
}

}  // namespace relic
