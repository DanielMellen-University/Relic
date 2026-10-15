#pragma once
#include "ast/ast.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace relic {

struct Value;
struct Env;

using List = std::vector<Value>;

// Map that keeps insertion order, so printing and `for k in map` are stable.
struct Map {
    std::vector<std::string> keys;
    std::unordered_map<std::string, Value> items;
    bool has(const std::string& k) const;
    void set(const std::string& k, Value v);
};

struct Function {
    std::string name;
    const Ast* node;  // the Fun node; the tree outlives the program run
    std::shared_ptr<Env> closure;
    std::string file;  // file the function was written in, for error locations
};

struct Builtin {
    std::string name;
    int arity;  // -1 for any number of arguments
    // Throws std::string on a bad argument; the caller adds the location.
    std::function<Value(std::vector<Value>&)> fn;
};

// A loaded file. Fields are read live from the module's top-level scope, so
// two modules that use each other see each other's names once they exist.
struct Module {
    std::string name;
    std::string file;
    std::shared_ptr<Env> scope;
};

struct NoneV {};

struct Value {
    std::variant<NoneV, bool, double, std::string, std::shared_ptr<List>, std::shared_ptr<Map>,
                 std::shared_ptr<Function>, std::shared_ptr<Builtin>, std::shared_ptr<Module>>
        v;

    Value() : v(NoneV{}) {}
    Value(bool b) : v(b) {}
    Value(double d) : v(d) {}
    Value(std::string s) : v(std::move(s)) {}
    Value(std::shared_ptr<List> l) : v(std::move(l)) {}
    Value(std::shared_ptr<Map> m) : v(std::move(m)) {}
    Value(std::shared_ptr<Function> f) : v(std::move(f)) {}
    Value(std::shared_ptr<Builtin> b) : v(std::move(b)) {}
    Value(std::shared_ptr<Module> m) : v(std::move(m)) {}

    template <class T> bool is() const { return std::holds_alternative<T>(v); }
    template <class T> const T& as() const { return std::get<T>(v); }
};

// Type name as shown in errors and by type(): none, bool, number, string, list, map, function, module.
const char* type_name(const Value& v);

// show: how print writes a value. repr: how a value looks inside a list or map
// (strings get quotes).
std::string show(const Value& v);
std::string repr(const Value& v);

bool equal(const Value& a, const Value& b);

}  // namespace relic
