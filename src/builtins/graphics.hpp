#pragma once
#include "interp/value.hpp"

#include <functional>
#include <string>
#include <vector>

namespace relic {

using AddBuiltin = std::function<void(const std::string&, int, std::function<Value(std::vector<Value>&)>)>;

// Registers the host_gfx_* built-ins (SDL2 when RELIC_HAVE_SDL is defined).
void add_graphics_builtins(const AddBuiltin& add);

}  // namespace relic
