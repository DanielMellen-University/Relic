#pragma once
#include "interp/value.hpp"

#include <vector>

namespace relic {

// host_program(): the path of this executable.
Value host_process_program();
// host_run(argv, cwd, stdin, env): run a process and return {code, out, err}.
// Throws std::string on a bad argument or if the program cannot be started.
Value host_process_run(std::vector<Value>& args);

}  // namespace relic
