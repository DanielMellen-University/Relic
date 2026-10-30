#pragma once
#include <cstddef>

namespace relic {

// The size in bytes of the current thread's native stack (capped at 256 MB).
std::size_t native_stack_size();

}  // namespace relic
