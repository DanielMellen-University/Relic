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
    ~List() override;  // frees deep nesting without deep C++ recursion
    void trace(std::vector<GcObj*>& out) const override;
    void clear_refs() override;
};

// Map that keeps insertion order, so printing and `for k in map` are stable.
// The order is a list of keys in which a removed key leaves a hole, and a
// key-to-position index (built at the first remove) finds the hole to make, so
// remove is O(1) on average instead of a search and a shift of the list. When
// holes are over half of the list it is compacted.
struct Map : GcObj {
    Map() = default;
    Map(const Map&) = default;
    Map(Map&&) = default;
    Map& operator=(const Map&) = default;
    Map& operator=(Map&&) = default;
    ~Map() override;
    std::unordered_map<std::string, Value> items;
    bool has(const std::string& k) const;
    void set(const std::string& k, Value v);
    // Removes k if it is there (true) and keeps the order of the rest.
    bool remove(const std::string& k);
    size_t size() const { return items.size(); }
    // f(key) for each key in order.
    template <class F> void each_key(F f) const {
        for (size_t i = 0; i < order_.size(); ++i)
            if (gone_.empty() || !gone_[i]) f(order_[i]);
    }
    std::vector<std::string> key_list() const;
    void trace(std::vector<GcObj*>& out) const override;
    void clear_refs() override;

private:
    std::vector<std::string> order_;              // keys in insertion order, with holes
    std::vector<char> gone_;                      // 1 for a hole; empty until the first remove
    std::unordered_map<std::string, size_t> pos_;  // key -> place in order_, once gone_ is in use
    size_t holes_ = 0;
    void compact();
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
