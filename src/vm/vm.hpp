#pragma once
// Bytecode VM (M10). Compiles the AST to a flat instruction list per function
// and runs it on a value stack. Variables resolve at compile time to scope
// slots instead of name lookups. Behavior, output and errors match the
// tree-walker; differences are listed in docs/ARCHITECTURE.md (Bytecode VM).
#include "ast/ast.hpp"

#include <ostream>
#include <string>

namespace relic {

// Runs a whole program. Throws RuntimeError like Interpreter::run.
// path: the file being run; `use` resolves against its folder.
void vm_run(const Ast& program, std::ostream& out, const std::string& path);

// Backs --bytecode: a listing of the compiled code.
std::string vm_disassemble(const Ast& program);

// Backs `relic check`: compiles without running and reports names that are
// never defined, top-level use before definition, and statements that always fail (like 'break' outside a
// loop), one diagnostic each. Gives the count; the text goes to `out`.
int vm_check(const Ast& program, const std::string& path, std::string& out);

}  // namespace relic
