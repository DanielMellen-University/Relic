#pragma once
// Bytecode VM (M10). Compiles the AST to a flat instruction list per function
// and runs it on a value stack. Variables resolve at compile time to scope
// slots instead of name lookups. Behavior, output and errors match the
// tree-walker; differences are listed in docs/DESIGN.md (Bytecode VM).
#include "ast/ast.hpp"

#include <ostream>

namespace relic {

// Runs a whole program. Throws RuntimeError like Interpreter::run.
void vm_run(const Ast& program, std::ostream& out);

// Backs --bytecode: a listing of the compiled code.
std::string vm_disassemble(const Ast& program);

}  // namespace relic
