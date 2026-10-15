// Built-in functions for Relic 1.0: print, len, append, str, keys, type.
#include "interp/interp.hpp"
#include "builtins/graphics.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

namespace relic {

std::vector<std::string>& script_args() {
    static std::vector<std::string> a;
    return a;
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
        if (v.is<std::string>()) return Value(static_cast<double>(v.as<std::string>().size()));
        if (v.is<std::shared_ptr<List>>())
            return Value(static_cast<double>(v.as<std::shared_ptr<List>>()->size()));
        if (v.is<std::shared_ptr<Map>>())
            return Value(static_cast<double>(v.as<std::shared_ptr<Map>>()->keys.size()));
        throw std::string("cannot get the length of a ") + type_name(v);
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

    add("type", 1, [](std::vector<Value>& a) { return Value(std::string(type_name(a[0]))); });
    // ---- M8 primitives. The std/ modules are written in Relic on top of these. ----
    auto num_arg = [](const Value& v, const char* what) {
        if (!v.is<double>()) throw std::string(what) + " must be a number, got " + type_name(v);
        return v.as<double>();
    };
    auto whole_arg = [num_arg](const Value& v, const char* what) {
        double d = num_arg(v, what);
        if (d != std::floor(d)) throw std::string(what) + " must be a whole number";
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
            size_t x = clampi(b, s.size()), y = clampi(e, s.size());
            return Value(y > x ? s.substr(x, y - x) : std::string());
        }
        if (a[0].is<std::shared_ptr<List>>()) {
            const List& l = *a[0].as<std::shared_ptr<List>>();
            size_t x = clampi(b, l.size()), y = clampi(e, l.size());
            auto r = std::make_shared<List>();
            for (size_t i = x; i < y; ++i) r->push_back(l[i]);
            return Value(r);
        }
        throw std::string("cannot slice a ") + type_name(a[0]);
    });
    add("ord", 1, [str_arg](std::vector<Value>& a) {
        const std::string& s = str_arg(a[0], "argument");
        if (s.size() != 1) throw std::string("needs a one-character string");
        return Value(static_cast<double>(static_cast<unsigned char>(s[0])));
    });
    add("chr", 1, [whole_arg](std::vector<Value>& a) {
        long long n = whole_arg(a[0], "argument");
        if (n < 0 || n > 255) throw std::string("code must be 0 to 255");
        return Value(std::string(1, static_cast<char>(n)));
    });
    add("num", 1, [str_arg](std::vector<Value>& a) {
        if (a[0].is<double>()) return a[0];
        const std::string& s = str_arg(a[0], "argument");
        size_t used = 0;
        double d = 0;
        try { d = std::stod(s, &used); } catch (...) { used = 0; }
        if (s.empty() || used != s.size() || std::isspace(static_cast<unsigned char>(s[0])))
            throw std::string("not a number: \"") + s + "\"";
        return Value(d);
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
        throw std::string("cannot remove from a ") + type_name(a[0]);
    });

    // Host-only pieces: files, clocks, float math. Named host_* so the
    // borrowed surface is easy to find and replace later.
    // args(): words after the file name on the command line.
    add("args", 0, [](std::vector<Value>&) {
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
        std::ifstream f(str_arg(a[0], "path"));
        return Value(static_cast<bool>(f));
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

    add_graphics_builtins(add);
}

}  // namespace relic
