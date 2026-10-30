#pragma once
#include "ast/ast.hpp"
#include "interp/value.hpp"

#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace relic {

struct RuntimeError : std::runtime_error {
    int line;
    int col;
    RuntimeError(int l, int c, const std::string& msg)
        : std::runtime_error(msg), line(l), col(c), text(msg) {}
    // what() ends at a NUL byte; a message can hold one (fail("a" + chr(0)), a bad key), so read this
    const std::string& message() const { return text; }
    std::string text;
    std::string file;  // set when the error happened in another file than the one being run
};

struct Env : GcObj {
    std::unordered_map<std::string, Value> vars;
    std::shared_ptr<Env> parent;
    explicit Env(std::shared_ptr<Env> p = nullptr) : parent(std::move(p)) {}
    void trace(std::vector<GcObj*>& out) const override {
        for (const auto& kv : vars) trace_value(kv.second, out);
        if (parent) out.push_back(parent.get());
    }
    void clear_refs() override {
        std::unordered_map<std::string, Value> gone;
        gone.swap(vars);
        auto p = std::move(parent);
    }
    std::unordered_map<std::string, std::string> hints;  // `as` types of names declared here
    Value* find(const std::string& name);
    Env* find_env(const std::string& name);  // the scope that holds name
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
    // The file being run. `use` paths resolve against its folder; "<repl>"
    // or empty means the current directory.
    void set_main_file(const std::string& path);
    // Built-in functions, shared with the bytecode VM.
    const std::shared_ptr<Env>& builtins() const { return builtins_; }

private:
    enum class Flow { Normal, Break, Next, Return };

    std::ostream& out_;
    std::shared_ptr<Env> builtins_;  // shared by every module
    std::shared_ptr<Env> globals_;   // top level of the main file
    std::string current_file_;
    std::unordered_map<std::string, std::shared_ptr<Module>> modules_;  // by resolved path
    std::vector<std::unique_ptr<Ast>> module_trees_;
    Value load_module(const Ast& use);
    Value ret_;
    const Ast* flow_node_ = nullptr;  // the break/next/return that caused the flow
    int depth_ = 0;
    // The tree-walker recurses on the native stack: a huge expression inside
    // deep recursion can run out of it before depth_ reaches its limit. eval
    // fails cleanly once the stack in use passes stack_budget_ (set by run()).
    const char* stack_base_ = nullptr;
    std::size_t stack_budget_ = 0;
    void check_stack(const Ast& at);

    Flow exec_block(const Ast& block, std::shared_ptr<Env> env);
    Flow exec(const Ast& n, const std::shared_ptr<Env>& env);
    Value eval(const Ast& n, const std::shared_ptr<Env>& env);
    Value binary(const Ast& n, const std::shared_ptr<Env>& env);
    Value call(const Ast& n, const std::shared_ptr<Env>& env);
    void assign(const Ast& target, Value v, const std::shared_ptr<Env>& env, const Ast& value_at);
    bool condition(const Ast& n, const std::shared_ptr<Env>& env, const char* what);
    void install_builtins();
};

}  // namespace relic
