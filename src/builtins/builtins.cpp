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
#include <cstdio>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define RELIC_ISATTY(fd) _isatty(fd)
#define RELIC_FILENO(f) _fileno(f)
#else
#include <unistd.h>
#define RELIC_ISATTY(fd) isatty(fd)
#define RELIC_FILENO(f) fileno(f)
#endif

namespace relic {

std::vector<std::string>& script_args() {
    static std::vector<std::string> a;
    return a;
}

// Text to a number: digits (with an optional sign) give an int, anything with
// a point or an exponent gives a float.
// num("..."): an optional sign, then digits (an int when it fits, else a float),
// "nan", "inf" or "infinity" in any case, hex "0x1F" (an int when it fits), or a
// decimal float. A float too small rounds like a literal (1e-400 is 0.0, 4.9e-324
// the smallest float above zero); one too large is not a number. The C runtime's
// rl_parse_num follows the same steps.
static bool same_word(const std::string& s, size_t i, const char* w) {
    size_t n = std::char_traits<char>::length(w);
    if (s.size() - i != n) return false;
    for (size_t k = 0; k < n; ++k)
        if (std::tolower(static_cast<unsigned char>(s[i + k])) != w[k]) return false;
    return true;
}

static Value parse_num(const std::string& s) {
    auto bad = [&]() { return std::string("not a number: \"") + s + "\""; };
    if (s.empty() || std::isspace(static_cast<unsigned char>(s[0])) || s.find('\0') != std::string::npos) throw bad();
    bool neg = s[0] == '-';
    size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i == s.size()) throw bad();
    if (same_word(s, i, "nan")) return Value(std::numeric_limits<double>::quiet_NaN());
    if (same_word(s, i, "inf") || same_word(s, i, "infinity"))
        return Value(neg ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity());
    bool hex = s.size() - i > 2 && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X');
    if (hex) {
        unsigned long long m = 0;
        bool fits = true, all = true;
        for (size_t j = i + 2; j < s.size(); ++j) {
            int c = std::tolower(static_cast<unsigned char>(s[j]));
            int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            if (d < 0) { all = false; break; }
            if (m > (~0ULL >> 4)) fits = false;
            else m = m * 16 + static_cast<unsigned>(d);
        }
        if (all && fits && m <= (neg ? 9223372036854775808ULL : 9223372036854775807ULL))
            return Value(neg ? static_cast<long long>(0ULL - m) : static_cast<long long>(m));
    } else {
        if (!std::isdigit(static_cast<unsigned char>(s[i])) && s[i] != '.') throw bad();
        bool digits = true;
        for (size_t j = i; j < s.size(); ++j)
            if (!std::isdigit(static_cast<unsigned char>(s[j]))) digits = false;
        if (digits) {
            errno = 0;
            long long v = std::strtoll(s.c_str(), nullptr, 10);
            if (errno != ERANGE) return Value(v);
        }
    }
    // A decimal float, a hex float (0x1.8p3) or a hex int past the int range.
    char* end = nullptr;
    errno = 0;
    double d = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size() || std::isinf(d)) throw bad();
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
        std::error_code ec;
        if (!f || std::filesystem::is_directory(a[0].as<std::string>(), ec))
            throw std::string("cannot open ") + a[0].as<std::string>();
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
        // checked in this order (as arguments of std::pow they ran right to left on GCC)
        double base = num_arg(a[0], "base");
        double power = num_arg(a[1], "power");
        return Value(std::pow(base, power));
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
    // host_read_line(prompt): the next line of standard input without its line
    // end ("\n" or "\r\n"), or none at the end of the input. When stdin is a
    // terminal the prompt is shown first (and a newline follows at the end of
    // the input); otherwise the prompt is ignored. Reads the C stdin in binary
    // mode, so the REPL and a program share one buffer and Windows leaves the
    // bytes alone.
    add("host_read_line", 1, [str_arg, this](std::vector<Value>& a) -> Value {
        const std::string& prompt = str_arg(a[0], "prompt");
        static bool binary = false;
        if (!binary) {
            binary = true;
#ifdef _WIN32
            _setmode(_fileno(stdin), _O_BINARY);
#endif
        }
        bool tty = RELIC_ISATTY(RELIC_FILENO(stdin)) != 0;
        if (tty && !prompt.empty()) out_ << prompt << std::flush;
        std::string line;
        int c;
        bool any = false;
        while ((c = std::getc(stdin)) != EOF) {
            any = true;
            if (c == '\n') {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                return Value(line);
            }
            line.push_back(static_cast<char>(c));
        }
        if (!any) {
            if (tty && !prompt.empty()) out_ << "\n" << std::flush;
            return Value();
        }
        return Value(line);
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
