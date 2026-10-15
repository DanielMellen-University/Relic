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
Value op_unary(const Ast& n, const Value& v);
Value op_range(const Ast& n, const Value& a, const Value& b);
Value op_field(const Ast& n, const Value& obj);
Value op_index(const Ast& n, const Value& obj, const Value& idx);
void op_set_field(const Ast& target, const Value& obj, Value v);
void op_set_index(const Ast& target, const Value& obj, const Value& idx, Value v);
bool op_condition(const Ast& n, const Value& v, const char* what);
List op_loop_items(const Ast& iterable, const Value& src);
Value op_call_builtin(const Ast& call, const Builtin& b, std::vector<Value>& args);
std::string op_arity_message(const std::string& name, size_t want, size_t got);

}  // namespace relic
