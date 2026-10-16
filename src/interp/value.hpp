#pragma once
#include "ast/ast.hpp"
#include "interp/gc.hpp"

#include <climits>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace relic {

struct Value;
struct Env;

struct List : std::vector<Value>, GcObj {
    using std::vector<Value>::vector;
    List() = default;
    List(const List&) = default;
    List(List&&) = default;
    List& operator=(const List&) = default;
    List& operator=(List&&) = default;
    void trace(std::vector<GcObj*>& out) const override;
    void clear_refs() override;
};

// Map that keeps insertion order, so printing and `for k in map` are stable.
struct Map : GcObj {
    std::vector<std::string> keys;
    std::unordered_map<std::string, Value> items;
    bool has(const std::string& k) const;
    void set(const std::string& k, Value v);
    void trace(std::vector<GcObj*>& out) const override;
    void clear_refs() override;
};

struct Function : GcObj {
    Function(std::string n, const Ast* nd, std::shared_ptr<Env> c, const void* p, std::shared_ptr<GcObj> s, std::string f)
        : name(std::move(n)), node(nd), closure(std::move(c)), proto(p), vm_scope(std::move(s)), file(std::move(f)) {}
    void trace(std::vector<GcObj*>& out) const override;
    void clear_refs() override;
    std::string name;
    const Ast* node;  // the Fun node; the tree outlives the program run
    std::shared_ptr<Env> closure;
    // Set for functions made by the bytecode VM: its Proto and captured Scope.
    const void* proto = nullptr;
    std::shared_ptr<GcObj> vm_scope;
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
    std::shared_ptr<Env> scope;  // tree-walker modules
    // VM modules: finds a top-level name, or null if it is not defined yet.
    std::function<const Value*(const std::string&)> lookup;
};

struct NoneV {};

struct Value {
    std::variant<NoneV, bool, double, long long, std::string, std::shared_ptr<List>, std::shared_ptr<Map>,
                 std::shared_ptr<Function>, std::shared_ptr<Builtin>, std::shared_ptr<Module>>
        v;

    Value() : v(NoneV{}) {}
    Value(bool b) : v(b) {}
    Value(double d) : v(d) {}
    Value(long long i) : v(i) {}
    Value(int i) : v(static_cast<long long>(i)) {}
    Value(std::string s) : v(std::move(s)) {}
    Value(std::shared_ptr<List> l) : v(std::move(l)) {}
    Value(std::shared_ptr<Map> m) : v(std::move(m)) {}
    Value(std::shared_ptr<Function> f) : v(std::move(f)) {}
    Value(std::shared_ptr<Builtin> b) : v(std::move(b)) {}
    Value(std::shared_ptr<Module> m) : v(std::move(m)) {}

    template <class T> bool is() const { return std::holds_alternative<T>(v); }
    template <class T> const T& as() const { return std::get<T>(v); }
};

// Type name as shown in errors and by type(): none, bool, int, float, string, list, map, function, module.
const char* type_name(const Value& v);
// The type name with "a" or "an" in front, for error messages: "an int", "a list".
std::string a_type(const Value& v);

// Numbers: int (64-bit) or float (64-bit).
inline bool is_num(const Value& v) { return v.is<double>() || v.is<long long>(); }
// Int math that reports overflow instead of wrapping. Portable (no compiler builtins).
inline bool add_overflows(long long x, long long y, long long* r) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_add_overflow(x, y, r);
#else
    if ((y > 0 && x > LLONG_MAX - y) || (y < 0 && x < LLONG_MIN - y)) return true;
    *r = x + y;
    return false;
#endif
}
inline bool sub_overflows(long long x, long long y, long long* r) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_sub_overflow(x, y, r);
#else
    if ((y < 0 && x > LLONG_MAX + y) || (y > 0 && x < LLONG_MIN + y)) return true;
    *r = x - y;
    return false;
#endif
}
inline bool mul_overflows(long long x, long long y, long long* r) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_mul_overflow(x, y, r);
#else
    if (x != 0 && y != 0) {
        if (x > 0 ? (y > 0 ? x > LLONG_MAX / y : y < LLONG_MIN / x)
                  : (y > 0 ? x < LLONG_MIN / y : y < LLONG_MAX / x))
            return true;
    }
    *r = x * y;
    return false;
#endif
}
inline double to_num(const Value& v) {
    return v.is<long long>() ? static_cast<double>(v.as<long long>()) : v.as<double>();
}

// show: how print writes a value. repr: how a value looks inside a list or map
// (strings get quotes).
std::string show(const Value& v);
std::string repr(const Value& v);

bool equal(const Value& a, const Value& b);

// Pushes the container a value holds, if any (for the cycle collector).
void trace_value(const Value& v, std::vector<GcObj*>& out);

// UTF-8: strings are bytes, but length, indexes, slices and loops count
// characters (code points). A byte that is not valid UTF-8 counts as one character.
size_t utf8_char_size(const std::string& s, size_t pos);  // bytes in the character at pos
size_t utf8_len(const std::string& s);
size_t utf8_offset(const std::string& s, size_t index);  // byte offset of character index, or s.size()
long utf8_decode(const std::string& s, size_t pos);       // code point at pos
std::string utf8_encode(long cp);

}  // namespace relic
