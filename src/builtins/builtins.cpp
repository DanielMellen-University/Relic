// Built-in functions for Relic 1.0: print, len, append, str, keys, type.
#include "interp/interp.hpp"
#include "builtins/graphics.hpp"

namespace relic { std::string std_root(); }

#include <chrono>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <algorithm>
#include <iostream>
#include <sstream>

namespace relic {

std::vector<std::string>& script_args() {
    static std::vector<std::string> a;
    return a;
}

// Text to a number: digits (with an optional sign) give an int, anything with
// a point or an exponent gives a float.
static Value parse_num(const std::string& s) {
    auto bad = [&]() { return std::string("not a number: \"") + s + "\""; };
    if (s.empty() || std::isspace(static_cast<unsigned char>(s[0]))) throw bad();
    size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    bool digits = i < s.size();
    for (size_t j = i; j < s.size(); ++j)
        if (!std::isdigit(static_cast<unsigned char>(s[j]))) digits = false;
    if (digits) {
        errno = 0;
        long long v = std::strtoll(s.c_str(), nullptr, 10);
        if (errno != ERANGE) return Value(v);
    }
    size_t used = 0;
    double d = 0;
    try { d = std::stod(s, &used); } catch (...) { used = 0; }
    if (used != s.size()) throw bad();
    return Value(d);
}

void Interpreter::install_builtins() {
    auto add = [&](const std::string& name, int arity, std::function<Value(std::vector<Value>&)> fn) {
        builtins_->vars[name] = Value(std::make_shared<Builtin>(Builtin{name, arity, std::move(fn)}));
    };

    // print(a, b, ...) writes the values separated by spaces, then a newline.
    add("print", -1, [this](std::vector<Value>& a) {
        for (size_t i = 0; i < a.size(); ++i) out_ << (i ? " " : "") << show(a[i]);
        out_ << "\n";
        return Value();
    });

    add("len", 1, [](std::vector<Value>& a) {
        const Value& v = a[0];
        if (v.is<std::string>()) return Value(static_cast<long long>(utf8_len(v.as<std::string>())));
        if (v.is<std::shared_ptr<List>>())
            return Value(static_cast<long long>(v.as<std::shared_ptr<List>>()->size()));
        if (v.is<std::shared_ptr<Map>>())
            return Value(static_cast<long long>(v.as<std::shared_ptr<Map>>()->keys.size()));
        throw std::string("cannot get the length of ") + a_type(v);
    });

    add("append", 2, [](std::vector<Value>& a) {
        if (!a[0].is<std::shared_ptr<List>>())
            throw std::string("first argument must be a list, got ") + type_name(a[0]);
        a[0].as<std::shared_ptr<List>>()->push_back(a[1]);
        return Value();
    });

    add("str", 1, [](std::vector<Value>& a) { return Value(show(a[0])); });

    add("keys", 1, [](std::vector<Value>& a) {
        if (!a[0].is<std::shared_ptr<Map>>())
            throw std::string("argument must be a map, got ") + type_name(a[0]);
        auto l = std::make_shared<List>();
        for (const auto& k : a[0].as<std::shared_ptr<Map>>()->keys) l->emplace_back(k);
        return Value(l);
    });

    // fail(message): raises an error that `try` / `catch` can catch.
    add("fail", 1, [](std::vector<Value>& a) -> Value {
        if (!a[0].is<std::string>()) throw std::string("fail(): message must be a string, got ") + type_name(a[0]);
        throw a[0].as<std::string>();
    });

    // gc(): runs the cycle collector now and gives how many containers it freed.
    add("gc", 0, [](std::vector<Value>&) { return Value(static_cast<long long>(gc_collect())); });

    add("type", 1, [](std::vector<Value>& a) { return Value(std::string(type_name(a[0]))); });
    // ---- M8 primitives. The std/ modules are written in Relic on top of these. ----
    auto num_arg = [](const Value& v, const char* what) {
        if (!is_num(v)) throw std::string(what) + " must be a number, got " + type_name(v);
        return to_num(v);
    };
    auto whole_arg = [num_arg](const Value& v, const char* what) {
        if (v.is<long long>()) return v.as<long long>();
        double d = num_arg(v, what);
        if (d != std::floor(d)) throw std::string(what) + " must be a whole number";
        // Past the int range the answer is the nearest int, not whatever the cast gives
        // (it was the lowest int on x86, so slice("abc", 0, 1e300) was "").
        if (d >= 9223372036854775808.0) return std::numeric_limits<long long>::max();
        if (d <= -9223372036854775808.0) return std::numeric_limits<long long>::min();
        return static_cast<long long>(d);
    };
    auto str_arg = [](const Value& v, const char* what) -> const std::string& {
        if (!v.is<std::string>()) throw std::string(what) + " must be a string, got " + type_name(v);
        return v.as<std::string>();
    };
    auto list_arg = [](const Value& v) -> List& {
        if (!v.is<std::shared_ptr<List>>()) throw std::string("first argument must be a list, got ") + type_name(v);
        return *v.as<std::shared_ptr<List>>();
    };

    // slice(x, start, end): part of a string or list, end left out. Ends are clamped.
    add("slice", 3, [whole_arg](std::vector<Value>& a) {
        long long b = whole_arg(a[1], "start"), e = whole_arg(a[2], "end");
        auto clampi = [](long long i, size_t n) { return static_cast<size_t>(i < 0 ? 0 : (static_cast<size_t>(i) > n ? n : i)); };
        if (a[0].is<std::string>()) {
            const std::string& s = a[0].as<std::string>();
            size_t n = utf8_len(s);
            size_t x = clampi(b, n), y = clampi(e, n);
            if (y <= x) return Value(std::string());
            size_t bx = utf8_offset(s, x), by = utf8_offset(s, y);
            return Value(s.substr(bx, by - bx));
        }
        if (a[0].is<std::shared_ptr<List>>()) {
            const List& l = *a[0].as<std::shared_ptr<List>>();
            size_t x = clampi(b, l.size()), y = clampi(e, l.size());
            auto r = std::make_shared<List>();
            for (size_t i = x; i < y; ++i) r->push_back(l[i]);
            return Value(r);
        }
        throw std::string("cannot slice ") + a_type(a[0]);
    });
    add("ord", 1, [str_arg](std::vector<Value>& a) {
        const std::string& s = str_arg(a[0], "argument");
        if (s.empty() || utf8_char_size(s, 0) != s.size()) throw std::string("needs a one-character string");
        return Value(static_cast<long long>(utf8_decode(s, 0)));
    });
    add("chr", 1, [whole_arg](std::vector<Value>& a) {
        long long n = whole_arg(a[0], "argument");
        if (n < 0 || n > 0x10FFFF || (n >= 0xD800 && n <= 0xDFFF))
            throw std::string("code must be 0 to 1114111 and not 55296 to 57343 (surrogates)");
        return Value(utf8_encode(static_cast<long>(n)));
    });
    add("num", 1, [str_arg](std::vector<Value>& a) {
        if (is_num(a[0])) return a[0];
        return parse_num(str_arg(a[0], "argument"));
    });
    // int(x): a number cut toward zero, or a string of digits. float(x): a float.
    add("int", 1, [](std::vector<Value>& a) -> Value {
        Value v = a[0];
        if (v.is<std::string>()) v = parse_num(v.as<std::string>());
        if (v.is<long long>()) return v;
        if (!v.is<double>()) throw std::string("cannot turn ") + a_type(v) + " into an int";
        double d = std::trunc(v.as<double>());
        if (std::isnan(d) || d >= 9.2233720368547758e18 || d < -9.2233720368547758e18)
            throw std::string("number is out of the int range");
        return Value(static_cast<long long>(d));
    });
    add("float", 1, [](std::vector<Value>& a) -> Value {
        Value v = a[0];
        if (v.is<std::string>()) v = parse_num(v.as<std::string>());
        if (!is_num(v)) throw std::string("cannot turn ") + a_type(v) + " into a float";
        return Value(to_num(v));
    });
    add("pop", 1, [list_arg](std::vector<Value>& a) {
        List& l = list_arg(a[0]);
        if (l.empty()) throw std::string("list is empty");
        Value v = l.back();
        l.pop_back();
        return v;
    });
    add("insert", 3, [list_arg, whole_arg](std::vector<Value>& a) {
        List& l = list_arg(a[0]);
        long long i = whole_arg(a[1], "index");
        if (i < 0 || static_cast<size_t>(i) > l.size()) throw std::string("index ") + std::to_string(i) + " is out of range";
        l.insert(l.begin() + i, a[2]);
        return Value();
    });
    // remove(list, index) gives back the item; remove(map, key) drops the key.
    add("remove", 2, [whole_arg](std::vector<Value>& a) {
        if (a[0].is<std::shared_ptr<List>>()) {
            List& l = *a[0].as<std::shared_ptr<List>>();
            long long i = whole_arg(a[1], "index");
            if (i < 0 || static_cast<size_t>(i) >= l.size()) throw std::string("index ") + std::to_string(i) + " is out of range";
            Value v = l[i];
            l.erase(l.begin() + i);
            return v;
        }
        if (a[0].is<std::shared_ptr<Map>>()) {
            if (!a[1].is<std::string>()) throw std::string("map keys must be strings");
            Map& m = *a[0].as<std::shared_ptr<Map>>();
            const std::string& k = a[1].as<std::string>();
            if (!m.has(k)) throw std::string("map has no key '") + k + "'";
            Value v = m.items[k];
            m.items.erase(k);
            for (size_t i = 0; i < m.keys.size(); ++i)
                if (m.keys[i] == k) { m.keys.erase(m.keys.begin() + i); break; }
            return v;
        }
        throw std::string("cannot remove from ") + a_type(a[0]);
    });

    // Host-only pieces: files, clocks, float math. Named host_* so the
    // borrowed surface is easy to find and replace later.
    // args(): words after the file name on the command line.
    add("args", 0, [](std::vector<Value>&) {
        auto l = std::make_shared<List>();
        for (const auto& s : script_args()) l->emplace_back(s);
        return Value(l);
    });
    add("host_args", 0, [](std::vector<Value>&) {
        auto l = std::make_shared<List>();
        for (const auto& s : script_args()) l->emplace_back(s);
        return Value(l);
    });
    // eprint(a, ...): like print, but to stderr.
    add("eprint", -1, [](std::vector<Value>& a) {
        for (size_t i = 0; i < a.size(); ++i) std::cerr << (i ? " " : "") << show(a[i]);
        std::cerr << "\n";
        return Value();
    });
    add("host_read_file", 1, [str_arg](std::vector<Value>& a) {
        std::ifstream f(str_arg(a[0], "path"), std::ios::binary);
        if (!f) throw std::string("cannot open ") + a[0].as<std::string>();
        std::stringstream ss;
        ss << f.rdbuf();
        return Value(ss.str());
    });
    add("host_write_file", 3, [str_arg](std::vector<Value>& a) {
        bool app = a[2].is<bool>() && a[2].as<bool>();
        std::ofstream f(str_arg(a[0], "path"), std::ios::binary | (app ? std::ios::app : std::ios::trunc));
        if (!f) throw std::string("cannot write ") + a[0].as<std::string>();
        f << str_arg(a[1], "text");
        return Value();
    });
    add("host_file_exists", 1, [str_arg](std::vector<Value>& a) {
        std::error_code ec;
        return Value(std::filesystem::exists(std::filesystem::u8path(str_arg(a[0], "path")), ec));
    });
    add("host_clock", 0, [](std::vector<Value>&) {
        using namespace std::chrono;
        return Value(duration<double>(steady_clock::now().time_since_epoch()).count());
    });
    add("host_now", 0, [](std::vector<Value>&) {
        using namespace std::chrono;
        return Value(duration<double>(system_clock::now().time_since_epoch()).count());
    });
    auto math1 = [&](const std::string& name, double (*f)(double)) {
        add(name, 1, [f, num_arg](std::vector<Value>& a) { return Value(f(num_arg(a[0], "argument"))); });
    };
    math1("host_floor", [](double x) { return std::floor(x); });
    math1("host_sqrt", [](double x) { return std::sqrt(x); });
    math1("host_sin", [](double x) { return std::sin(x); });
    math1("host_cos", [](double x) { return std::cos(x); });
    math1("host_exp", [](double x) { return std::exp(x); });
    math1("host_log", [](double x) { return std::log(x); });
    add("host_pow", 2, [num_arg](std::vector<Value>& a) {
        return Value(std::pow(num_arg(a[0], "base"), num_arg(a[1], "power")));
    });

    // ---- M12 host pieces for MONOLITH 2: folders, process, and a way for the
    // self-hosted interpreter to reach any host_* built-in. ----
    namespace fs = std::filesystem;
    add("host_list_dir", 1, [str_arg](std::vector<Value>& a) {
        const std::string& p = str_arg(a[0], "path");
        std::error_code ec;
        std::vector<std::string> names;
        for (fs::directory_iterator it(fs::u8path(p), ec), end; !ec && it != end; it.increment(ec))
            names.push_back(it->path().filename().u8string());
        if (ec) throw std::string("cannot list ") + p;
        std::sort(names.begin(), names.end());
        auto l = std::make_shared<List>();
        for (auto& n : names) l->emplace_back(n);
        return Value(l);
    });
    add("host_is_dir", 1, [str_arg](std::vector<Value>& a) {
        std::error_code ec;
        return Value(fs::is_directory(fs::u8path(str_arg(a[0], "path")), ec));
    });
    add("host_make_dir", 1, [str_arg](std::vector<Value>& a) {
        const std::string& p = str_arg(a[0], "path");
        std::error_code ec;
        fs::create_directories(fs::u8path(p), ec);
        if (ec || !fs::is_directory(fs::u8path(p))) throw std::string("cannot make folder ") + p;
        return Value();
    });
    // Removes a file or an empty folder.
    add("host_remove", 1, [str_arg](std::vector<Value>& a) {
        const std::string& p = str_arg(a[0], "path");
        std::error_code ec;
        if (!fs::remove(fs::u8path(p), ec) || ec) throw std::string("cannot remove ") + p;
        return Value();
    });
    add("host_rename", 2, [str_arg](std::vector<Value>& a) {
        const std::string& from = str_arg(a[0], "from");
        std::error_code ec;
        fs::rename(fs::u8path(from), fs::u8path(str_arg(a[1], "to")), ec);
        if (ec) throw std::string("cannot rename ") + from;
        return Value();
    });
    add("host_file_size", 1, [str_arg](std::vector<Value>& a) {
        const std::string& p = str_arg(a[0], "path");
        std::error_code ec;
        auto n = fs::file_size(fs::u8path(p), ec);
        if (ec) throw std::string("cannot get the size of ") + p;
        return Value(static_cast<long long>(n));
    });
    add("host_cwd", 0, [](std::vector<Value>&) { return Value(fs::current_path().generic_u8string()); });
    add("host_env", 1, [str_arg](std::vector<Value>& a) {
        const char* v = std::getenv(str_arg(a[0], "name").c_str());
        return v ? Value(std::string(v)) : Value();
    });
    add("host_exit", 1, [whole_arg, this](std::vector<Value>& a) -> Value {
        long long code = whole_arg(a[0], "code");
        // Program output is collected in a buffer until the run ends; write it now.
        if (auto* buf = dynamic_cast<std::ostringstream*>(&out_)) std::cout << buf->str();
        else out_.flush();
        std::cout.flush();
        std::exit(static_cast<int>(code));
    });
    add("host_sleep", 1, [num_arg](std::vector<Value>& a) {
        double ms = num_arg(a[0], "milliseconds");
        if (ms > 0) std::this_thread::sleep_for(std::chrono::microseconds(static_cast<long long>(ms * 1000)));
        return Value();
    });
    // host_same(a, b): true when a and b are the very same list, map or
    // function, not just equal. The self-hosted interpreter uses it to print
    // lists that hold themselves.
    add("host_same", 2, [](std::vector<Value>& a) {
        const Value &x = a[0], &y = a[1];
        if (x.v.index() != y.v.index()) return Value(false);
        if (x.is<std::shared_ptr<List>>()) return Value(x.as<std::shared_ptr<List>>() == y.as<std::shared_ptr<List>>());
        if (x.is<std::shared_ptr<Map>>()) return Value(x.as<std::shared_ptr<Map>>() == y.as<std::shared_ptr<Map>>());
        return Value(false);
    });
    add("host_std_root", 0, [](std::vector<Value>&) { return Value(std_root()); });
    // host_names(): [name, arity] for every host_* built-in.
    add("host_names", 0, [this](std::vector<Value>&) {
        std::vector<std::string> names;
        for (auto& kv : builtins_->vars)
            if (kv.first.rfind("host_", 0) == 0) names.push_back(kv.first);
        std::sort(names.begin(), names.end());
        auto l = std::make_shared<List>();
        for (auto& n : names) {
            auto pair = std::make_shared<List>();
            pair->emplace_back(n);
            pair->emplace_back(static_cast<long long>(builtins_->vars[n].as<std::shared_ptr<Builtin>>()->arity));
            l->emplace_back(pair);
        }
        return Value(l);
    });
    // host_call(name, args): calls a host_* built-in with a list of arguments.
    add("host_call", 2, [this, str_arg, list_arg](std::vector<Value>& a) {
        const std::string& n = str_arg(a[0], "name");
        auto it = builtins_->vars.find(n);
        if (n.rfind("host_", 0) != 0 || it == builtins_->vars.end()) throw std::string("no host built-in ") + n;
        std::vector<Value> args = list_arg(a[1]);
        auto b = it->second.as<std::shared_ptr<Builtin>>();
        if (b->arity >= 0 && static_cast<size_t>(b->arity) != args.size())
            throw n + "() takes " + std::to_string(b->arity) + " arguments";
        return b->fn(args);
    });

    add_graphics_builtins(add);
}

}  // namespace relic
