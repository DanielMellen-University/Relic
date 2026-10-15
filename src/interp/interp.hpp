#pragma once
#include "ast/ast.hpp"
#include "interp/value.hpp"

#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace relic {

struct RuntimeError : std::runtime_error {
    int line;
    int col;
    RuntimeError(int l, int c, const std::string& msg)
        : std::runtime_error(msg), line(l), col(c) {}
};

struct Env {
    std::unordered_map<std::string, Value> vars;
    std::shared_ptr<Env> parent;
    explicit Env(std::shared_ptr<Env> p = nullptr) : parent(std::move(p)) {}
    Value* find(const std::string& name);
};

// Tree-walk interpreter. Keeps its global scope between run() calls, so the
// REPL (M5) can feed it one input at a time. The trees passed to run() must
// stay alive as long as the interpreter, since functions point into them.
class Interpreter {
public:
    explicit Interpreter(std::ostream& out);
    // Runs every statement. If `last` is set and the final statement is a bare
    // expression, its value is stored there (the REPL echoes it).
    void run(const Ast& program, Value* last = nullptr);  // throws RuntimeError

private:
    enum class Flow { Normal, Break, Next, Return };

    std::ostream& out_;
    std::shared_ptr<Env> globals_;
    Value ret_;
    const Ast* flow_node_ = nullptr;  // the break/next/return that caused the flow
    int depth_ = 0;

    Flow exec_block(const Ast& block, std::shared_ptr<Env> env);
    Flow exec(const Ast& n, const std::shared_ptr<Env>& env);
    Value eval(const Ast& n, const std::shared_ptr<Env>& env);
    Value binary(const Ast& n, const std::shared_ptr<Env>& env);
    Value call(const Ast& n, const std::shared_ptr<Env>& env);
    void assign(const Ast& target, Value v, const std::shared_ptr<Env>& env);
    bool condition(const Ast& n, const std::shared_ptr<Env>& env, const char* what);
    void install_builtins();
};

}  // namespace relic
