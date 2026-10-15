#pragma once
// Operations shared by the tree-walker (interp.cpp) and the bytecode VM
// (vm/vm.cpp), so both give the same results and the same error messages.
// Each takes the AST node it reports errors at; all throw RuntimeError.
#include "ast/ast.hpp"
#include "interp/value.hpp"

namespace relic {

constexpr int kMaxCallDepth = 800;

[[noreturn]] void fail_at(const Ast& at, const std::string& msg);
Value op_binary(const Ast& n, const Value& a, const Value& b);  // not and/or
Value op_math(const Ast& n, const Value& a, const Value& b);  // + - * / % on numbers
Value op_number(const Ast& n);  // a Number literal: int, or float if it has a point
Value op_unary(const Ast& n, const Value& v);
Value op_range(const Ast& n, const Value& a, const Value& b);
Value op_field(const Ast& n, const Value& obj);
Value op_index(const Ast& n, const Value& obj, const Value& idx);
void op_set_field(const Ast& target, const Value& obj, Value v);
void op_set_index(const Ast& target, const Value& obj, const Value& idx, Value v);
bool op_condition(const Ast& n, const Value& v, const char* what);
List op_loop_items(const Ast& iterable, const Value& src);
Value op_call_builtin(const Ast& call, const Builtin& b, std::vector<Value>& args);
struct RuntimeError;
// The value a `catch name` block sees: {message, line, col}.
Value op_error_value(const RuntimeError& e);
// `as` hints: does v fit the type name? op_check_type fails with
// "<what> <type>, got <actual>" when it does not.
bool op_type_ok(const std::string& type, const Value& v);
void op_check_type(const Ast& at, const std::string& type, const Value& v, const std::string& what);
// The hint of a Let, a Fun (its return) or a parameter Name, or "" if none.
const std::string& op_hint(const Ast& n);
std::string op_arity_message(const std::string& name, size_t want, size_t got);

// Modules: the file a `use` path names, seen from the importing file, and the
// key that identifies a file in the module cache.
std::string module_path(const std::string& importer, const std::string& use_path);
std::string key_of(const std::string& file);

}  // namespace relic
