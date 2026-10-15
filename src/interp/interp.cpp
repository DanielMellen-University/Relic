#include "interp/interp.hpp"
#include "interp/ops.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

namespace relic {

Env* Env::find_env(const std::string& name) {
    for (Env* e = this; e; e = e->parent.get())
        if (e->vars.count(name)) return e;
    return nullptr;
}

void Function::trace(std::vector<GcObj*>& out) const {
    if (closure) out.push_back(closure.get());
    if (vm_scope) out.push_back(vm_scope.get());
}

Value* Env::find(const std::string& name) {
    for (Env* e = this; e; e = e->parent.get()) {
        auto it = e->vars.find(name);
        if (it != e->vars.end()) return &it->second;
    }
    return nullptr;
}

namespace {

constexpr int kMaxDepth = 800;

[[noreturn]] void fail(const Ast& at, const std::string& msg) {
    throw RuntimeError(at.line, at.col, msg);
}

// Turns a number into a whole-number index, or fails.
long long whole(const Ast& at, const Value& v, const char* what) {
    if (v.is<long long>()) return v.as<long long>();
    if (!v.is<double>()) fail(at, std::string(what) + " must be a number, got " + type_name(v));
    double d = v.as<double>();
    if (d != std::floor(d) || std::fabs(d) > 9e15)
        fail(at, std::string(what) + " must be a whole number");
    return static_cast<long long>(d);
}

size_t list_slot(const Ast& at, const Value& idx, size_t size, const char* what = "list index") {
    long long i = whole(at, idx, what);
    if (i < 0 || static_cast<size_t>(i) >= size)
        fail(at, std::string(what) + " " + std::to_string(i) + " is out of range (size " +
                     std::to_string(size) + ")");
    return static_cast<size_t>(i);
}

}  // namespace

Value op_binary(const Ast& n, const Value& a, const Value& b) {
    const std::string& op = n.text;
    auto mismatch = [&](const char* verb) -> Value {
        fail(n, std::string("cannot ") + verb + " " + a_type(a) + " and " + a_type(b));
    };
    if (op == "==") return Value(equal(a, b));
    if (op == "!=") return Value(!equal(a, b));
    if (op == "in") {
        if (b.is<std::shared_ptr<List>>()) {
            for (const auto& x : *b.as<std::shared_ptr<List>>())
                if (equal(a, x)) return Value(true);
            return Value(false);
        }
        if (b.is<std::shared_ptr<Map>>()) {
            if (!a.is<std::string>()) fail(n, "map keys must be strings");
            return Value(b.as<std::shared_ptr<Map>>()->has(a.as<std::string>()));
        }
        if (b.is<std::string>()) {
            if (!a.is<std::string>()) fail(n, "can only look for a string inside a string");
            return Value(b.as<std::string>().find(a.as<std::string>()) != std::string::npos);
        }
        fail(n, std::string("cannot look inside ") + a_type(b));
    }
    if (op == "+") {
        if (is_num(a) && is_num(b)) return op_math(n, a, b);
        if (a.is<std::string>() && b.is<std::string>())
            return Value(a.as<std::string>() + b.as<std::string>());
        if (a.is<std::shared_ptr<List>>() && b.is<std::shared_ptr<List>>()) {
            auto l = std::make_shared<List>(*a.as<std::shared_ptr<List>>());
            for (const auto& x : *b.as<std::shared_ptr<List>>()) l->push_back(x);
            return Value(l);
        }
        return mismatch("add");
    }
    if (op == "<" || op == "<=" || op == ">" || op == ">=") {
        int c;
        if (a.is<long long>() && b.is<long long>()) {
            long long x = a.as<long long>(), y = b.as<long long>();
            c = x < y ? -1 : (x > y ? 1 : 0);
        } else if (is_num(a) && is_num(b)) {
            double x = to_num(a), y = to_num(b);
            c = x < y ? -1 : (x > y ? 1 : 0);
        } else if (a.is<std::string>() && b.is<std::string>()) {
            c = a.as<std::string>().compare(b.as<std::string>());
        } else {
            return mismatch("compare");
        }
        if (op == "<") return Value(c < 0);
        if (op == "<=") return Value(c <= 0);
        if (op == ">") return Value(c > 0);
        return Value(c >= 0);
    }
    if (!is_num(a) || !is_num(b)) {
        if (op == "-") return mismatch("subtract");
        if (op == "*") return mismatch("multiply");
        return mismatch("divide");
    }
    return op_math(n, a, b);
}

// + - * / % on two numbers. int with int stays int (except /, which always
// gives a float); anything with a float gives a float.
Value op_math(const Ast& n, const Value& a, const Value& b) {
    const char o = n.text[0];
    if (a.is<long long>() && b.is<long long>()) {
        long long x = a.as<long long>(), y = b.as<long long>(), r = 0;
        switch (o) {
            case '+': if (__builtin_add_overflow(x, y, &r)) fail(n, "int overflow"); return Value(r);
            case '-': if (__builtin_sub_overflow(x, y, &r)) fail(n, "int overflow"); return Value(r);
            case '*': if (__builtin_mul_overflow(x, y, &r)) fail(n, "int overflow"); return Value(r);
            case '/':
                if (y == 0) fail(n, "division by zero");
                return Value(static_cast<double>(x) / static_cast<double>(y));
            default:
                if (y == 0) fail(n, "division by zero");
                if (y == -1) return Value(0);
                r = x % y;
                if (r != 0 && ((r < 0) != (y < 0))) r += y;  // result takes the sign of the divisor
                return Value(r);
        }
    }
    double x = to_num(a), y = to_num(b);
    switch (o) {
        case '+': return Value(x + y);
        case '-': return Value(x - y);
        case '*': return Value(x * y);
    }
    if (y == 0) fail(n, "division by zero");
    if (o == '/') return Value(x / y);
    double r = std::fmod(x, y);
    if (r != 0 && ((r < 0) != (y < 0))) r += y;
    return Value(r);
}

Value op_number(const Ast& n) {
    if (n.text.find('.') != std::string::npos) return Value(std::stod(n.text));
    errno = 0;
    long long i = std::strtoll(n.text.c_str(), nullptr, 10);
    if (errno == ERANGE) fail(n, "int is too large (the limit is 9223372036854775807)");
    return Value(i);
}

Value op_unary(const Ast& n, const Value& v) {
    if (n.text == "-") {
        if (v.is<long long>()) {
            long long r;
            if (__builtin_sub_overflow(0LL, v.as<long long>(), &r)) fail(n, "int overflow");
            return Value(r);
        }
        if (!v.is<double>()) fail(n, std::string("cannot negate ") + a_type(v));
        return Value(-v.as<double>());
    }
    if (!v.is<bool>()) fail(n, std::string("'not' needs true or false, got ") + type_name(v));
    return Value(!v.as<bool>());
}

Value op_field(const Ast& n, const Value& obj) {
            if (obj.is<std::shared_ptr<Module>>()) {
                const Module& m = *obj.as<std::shared_ptr<Module>>();
                if (!n.text.empty() && n.text[0] == '_')
                    fail(n, "'" + n.text + "' is private to module '" + m.name + "'");
                if (m.lookup) {
                    const Value* v = m.lookup(n.text);
                    if (!v) fail(n, "module '" + m.name + "' has no '" + n.text + "' (yet)");
                    return *v;
                }
                auto it = m.scope->vars.find(n.text);
                if (it == m.scope->vars.end())
                    fail(n, "module '" + m.name + "' has no '" + n.text + "' (yet)");
                return it->second;
            }
            if (!obj.is<std::shared_ptr<Map>>())
                fail(n, std::string("cannot read a field of ") + a_type(obj));
            const Map& m = *obj.as<std::shared_ptr<Map>>();
            if (!m.has(n.text)) fail(n, "map has no key '" + n.text + "'");
            return m.items.at(n.text);
}

Value op_index(const Ast& n, const Value& obj, const Value& idx) {
            if (obj.is<std::shared_ptr<List>>()) {
                const List& l = *obj.as<std::shared_ptr<List>>();
                return l[list_slot(*n.kids[1], idx, l.size())];
            }
            if (obj.is<std::string>()) {
                const std::string& s = obj.as<std::string>();
                long long i = whole(*n.kids[1], idx, "string index");
                size_t at = i < 0 ? s.size() : utf8_offset(s, static_cast<size_t>(i));
                if (at >= s.size()) list_slot(*n.kids[1], idx, utf8_len(s), "string index");  // fails
                return Value(s.substr(at, utf8_char_size(s, at)));
            }
            if (obj.is<std::shared_ptr<Map>>()) {
                if (!idx.is<std::string>()) fail(*n.kids[1], "map keys must be strings");
                const Map& m = *obj.as<std::shared_ptr<Map>>();
                const std::string& k = idx.as<std::string>();
                if (!m.has(k)) fail(*n.kids[1], "map has no key '" + k + "'");
                return m.items.at(k);
            }
            fail(n, std::string("cannot index ") + a_type(obj));
}

void op_set_field(const Ast& target, const Value& obj, Value v) {
    if (obj.is<std::shared_ptr<Module>>())
        fail(target, "cannot change a module's names from outside; give it a function that does it");
    if (!obj.is<std::shared_ptr<Map>>())
        fail(target, std::string("cannot set a field on ") + a_type(obj));
    obj.as<std::shared_ptr<Map>>()->set(target.text, std::move(v));
}

void op_set_index(const Ast& target, const Value& obj, const Value& idx, Value v) {
    if (obj.is<std::shared_ptr<List>>()) {
        List& l = *obj.as<std::shared_ptr<List>>();
        l[list_slot(*target.kids[1], idx, l.size())] = std::move(v);
    } else if (obj.is<std::shared_ptr<Map>>()) {
        if (!idx.is<std::string>()) fail(*target.kids[1], "map keys must be strings");
        obj.as<std::shared_ptr<Map>>()->set(idx.as<std::string>(), std::move(v));
    } else {
        fail(target, std::string("cannot set an index on ") + a_type(obj));
    }
}

[[noreturn]] void fail_at(const Ast& at, const std::string& msg) { fail(at, msg); }

Value op_range(const Ast& n, const Value& av, const Value& bv) {
    long long a = whole(*n.kids[0], av, "range start");
    long long b = whole(*n.kids[1], bv, "range end");
    if (b - a > 10000000) fail(n, "range is too large");
    auto l = std::make_shared<List>();
    for (long long i = a; i < b; ++i) l->emplace_back(static_cast<long long>(i));
    return Value(l);
}

bool op_condition(const Ast& n, const Value& v, const char* what) {
    if (!v.is<bool>())
        fail(n, std::string(what) + " must be true or false, got " + type_name(v));
    return v.as<bool>();
}

List op_loop_items(const Ast& at, const Value& src) {
    List items;
    if (src.is<std::shared_ptr<List>>()) {
        items = *src.as<std::shared_ptr<List>>();
    } else if (src.is<std::string>()) {
        const std::string& s = src.as<std::string>();
        for (size_t i = 0; i < s.size();) {
            size_t n = utf8_char_size(s, i);
            items.emplace_back(s.substr(i, n));
            i += n;
        }
    } else if (src.is<std::shared_ptr<Map>>()) {
        for (const auto& k : src.as<std::shared_ptr<Map>>()->keys) items.emplace_back(k);
    } else {
        fail(at, std::string("cannot loop over ") + a_type(src));
    }
    return items;
}

bool op_type_ok(const std::string& t, const Value& v) {
    if (t == "any") return true;
    if (t == "number") return is_num(v);
    return t == type_name(v);
}

void op_check_type(const Ast& at, const std::string& t, const Value& v, const std::string& what) {
    if (!op_type_ok(t, v)) fail(at, what + " " + t + ", got " + type_name(v));
}

const std::string& op_hint(const Ast& n) {
    static const std::string none;
    size_t i = n.kind == Node::Let ? 1 : n.kind == Node::Fun ? 2 : 0;
    if (n.kids.size() > i && n.kids[i]->kind == Node::Type) return n.kids[i]->text;
    return none;
}

std::string op_arity_message(const std::string& name, size_t want, size_t got) {
    return name + "() takes " + std::to_string(want) + " argument" + (want == 1 ? "" : "s") +
           ", got " + std::to_string(got);
}

Value op_call_builtin(const Ast& n, const Builtin& b, std::vector<Value>& args) {
    if (b.arity >= 0 && static_cast<int>(args.size()) != b.arity)
        fail(n, op_arity_message(b.name, static_cast<size_t>(b.arity), args.size()));
    try {
        return b.fn(args);
    } catch (const std::string& msg) {
        if (b.name == "fail") fail(n, msg);  // fail("msg") raises the message as written
        fail(n, b.name + "(): " + msg);
    }
}

Value op_error_value(const RuntimeError& e) {
    auto m = std::make_shared<Map>();
    m->set("message", Value(std::string(e.what())));
    m->set("line", Value(static_cast<long long>(e.line)));
    m->set("col", Value(static_cast<long long>(e.col)));
    return Value(m);
}

Interpreter::Interpreter(std::ostream& out)
    : out_(out), builtins_(std::make_shared<Env>()), globals_(std::make_shared<Env>(builtins_)) {
    install_builtins();
}

namespace fs = std::filesystem;

// Folder that `use` paths in `file` resolve against.
std::string folder_of(const std::string& file) {
    if (file.empty() || file == "<repl>") return "";
    size_t cut = file.find_last_of("/\\");
    return cut == std::string::npos ? "" : file.substr(0, cut + 1);
}

// Where `use std/...` looks: $RELIC_STD if set, else the std/ folder of the
// source tree this host was built from.
std::string std_root() {
    const char* env = std::getenv("RELIC_STD");
    std::string r = env && *env ? env : RELIC_STD_DIR;
    if (!r.empty() && r.back() != '/' && r.back() != '\\') r += '/';
    return r;
}

std::string key_of(const std::string& file) {
    std::error_code ec;
    fs::path k = fs::weakly_canonical(fs::path(file), ec);
    return ec ? file : k.generic_string();
}

std::string module_path(const std::string& importer, const std::string& use_path) {
    return use_path.rfind("std/", 0) == 0 ? std_root() + use_path.substr(4) + ".relic"
                                          : folder_of(importer) + use_path + ".relic";
}

void Interpreter::set_main_file(const std::string& path) {
    current_file_ = path;
    if (path.empty() || path == "<repl>") return;
    // Register the main file, so a module that uses it back gets this same
    // scope instead of running the file a second time.
    std::string name = fs::path(path).stem().string();
    modules_[key_of(path)] = std::make_shared<Module>(Module{name, path, globals_, {}});
}

Value Interpreter::load_module(const Ast& use) {
    std::string file = use.text.rfind("std/", 0) == 0 ? std_root() + use.text.substr(4) + ".relic"
                                                      : folder_of(current_file_) + use.text + ".relic";
    std::string key = key_of(file);
    auto it = modules_.find(key);
    if (it != modules_.end()) return Value(it->second);  // loaded, or loading (a cycle)

    std::ifstream in(file, std::ios::binary);
    if (!in) fail(use, "cannot find module '" + use.text + "' (no file " + use.text + ".relic)");
    std::stringstream ss;
    ss << in.rdbuf();

    std::string name = use.text.substr(use.text.find_last_of('/') + 1);
    auto mod = std::make_shared<Module>(Module{name, file, std::make_shared<Env>(builtins_), {}});
    modules_[key] = mod;  // before running, so cycles find it

    std::string saved = current_file_;
    current_file_ = file;
    struct Restore { std::string& cur; std::string old; ~Restore() { cur = old; } } restore{current_file_, saved};
    try {
        module_trees_.push_back(parse(lex(ss.str())));
    } catch (const LexError& e) {
        RuntimeError r(e.line, e.col, e.what());
        r.file = file;
        throw r;
    } catch (const ParseError& e) {
        RuntimeError r(e.line, e.col, e.what());
        r.file = file;
        throw r;
    }
    const Ast& prog = *module_trees_.back();
    try {
        for (const auto& st : prog.kids) {
            Flow f = exec(*st, mod->scope);
            if (f == Flow::Return) fail(*flow_node_, "'return' outside a function");
            if (f == Flow::Break) fail(*flow_node_, "'break' outside a loop");
            if (f == Flow::Next) fail(*flow_node_, "'next' outside a loop");
        }
    } catch (RuntimeError& e) {
        if (e.file.empty()) e.file = file;
        throw;
    }
    return Value(mod);
}

void Interpreter::run(const Ast& program, Value* last) {
    for (size_t i = 0; i < program.kids.size(); ++i) {
        const Ast& s = *program.kids[i];
        if (last && i + 1 == program.kids.size() && s.kind == Node::ExprStmt) {
            *last = eval(*s.kids[0], globals_);
            return;
        }
        Flow f = exec(s, globals_);
        if (f == Flow::Return) fail(*flow_node_, "'return' outside a function");
        if (f == Flow::Break) fail(*flow_node_, "'break' outside a loop");
        if (f == Flow::Next) fail(*flow_node_, "'next' outside a loop");
    }
}

Interpreter::Flow Interpreter::exec_block(const Ast& block, std::shared_ptr<Env> env) {
    for (const auto& s : block.kids) {
        Flow f = exec(*s, env);
        if (f != Flow::Normal) return f;
    }
    return Flow::Normal;
}

bool Interpreter::condition(const Ast& n, const std::shared_ptr<Env>& env, const char* what) {
    Value v = eval(n, env);
    if (!v.is<bool>())
        fail(n, std::string(what) + " must be true or false, got " + type_name(v));
    return v.as<bool>();
}

Interpreter::Flow Interpreter::exec(const Ast& n, const std::shared_ptr<Env>& env) {
    switch (n.kind) {
        case Node::Let: {
            Value v = eval(*n.kids[0], env);
            const std::string& hint = op_hint(n);
            if (!hint.empty()) {
                op_check_type(*n.kids[0], hint, v, "'" + n.text + "' must be");
                env->hints[n.text] = hint;
            } else if (!env->hints.empty()) {
                env->hints.erase(n.text);
            }
            env->vars[n.text] = std::move(v);
            return Flow::Normal;
        }
        case Node::Assign:
            assign(*n.kids[0], eval(*n.kids[1], env), env, *n.kids[1]);
            return Flow::Normal;
        case Node::ExprStmt:
            eval(*n.kids[0], env);
            return Flow::Normal;
        case Node::Fun: {
            auto f = std::make_shared<Function>(n.text, &n, env, nullptr, nullptr, std::string());
            f->file = current_file_;
            env->vars[n.text] = Value(f);
            return Flow::Normal;
        }
        case Node::If:
            if (condition(*n.kids[0], env, "'if' condition"))
                return exec_block(*n.kids[1], std::make_shared<Env>(env));
            if (n.kids.size() > 2) {
                const Ast& alt = *n.kids[2];
                if (alt.kind == Node::If) return exec(alt, env);
                return exec_block(alt, std::make_shared<Env>(env));
            }
            return Flow::Normal;
        case Node::While:
            while (condition(*n.kids[0], env, "'while' condition")) {
                gc_maybe();
                Flow f = exec_block(*n.kids[1], std::make_shared<Env>(env));
                if (f == Flow::Break) break;
                if (f == Flow::Return) return f;
            }
            return Flow::Normal;
        case Node::For: {
            Value src = eval(*n.kids[0], env);
            List items = op_loop_items(*n.kids[0], src);  // a copy: changes in the loop do not affect it
            for (auto& item : items) {
                gc_maybe();
                auto scope = std::make_shared<Env>(env);
                scope->vars[n.text] = item;
                Flow f = exec_block(*n.kids[1], scope);
                if (f == Flow::Break) break;
                if (f == Flow::Return) return f;
            }
            return Flow::Normal;
        }
        case Node::Return:
            ret_ = n.kids.empty() ? Value() : eval(*n.kids[0], env);
            flow_node_ = &n;
            return Flow::Return;
        case Node::Use: {
            Value m = load_module(n);
            env->vars[n.kids.empty() ? m.as<std::shared_ptr<Module>>()->name : n.kids[0]->text] = m;
            return Flow::Normal;
        }
        case Node::Try: {
            int saved_depth = depth_;
            try {
                return exec_block(*n.kids[0], std::make_shared<Env>(env));
            } catch (RuntimeError& e) {
                depth_ = saved_depth;
                auto scope = std::make_shared<Env>(env);
                if (!n.text.empty()) scope->vars[n.text] = op_error_value(e);
                return exec_block(*n.kids[1], scope);
            }
        }
        case Node::Break:
            flow_node_ = &n;
            return Flow::Break;
        case Node::Next:
            flow_node_ = &n;
            return Flow::Next;
        default:
            fail(n, std::string("cannot run a ") + node_name(n.kind) + " here");
    }
}

void Interpreter::assign(const Ast& target, Value v, const std::shared_ptr<Env>& env, const Ast& value_at) {
    if (target.kind == Node::Name) {
        Env* e = env->find_env(target.text);
        if (!e) fail(target, "'" + target.text + "' is not declared; use 'let " + target.text + " = ...'");
        if (!e->hints.empty()) {
            auto h = e->hints.find(target.text);
            if (h != e->hints.end()) op_check_type(value_at, h->second, v, "'" + target.text + "' must be");
        }
        e->vars[target.text] = std::move(v);
        return;
    }
    Value obj = eval(*target.kids[0], env);
    if (target.kind == Node::Field) return op_set_field(target, obj, std::move(v));
    Value idx = eval(*target.kids[1], env);
    op_set_index(target, obj, idx, std::move(v));
}

Value Interpreter::eval(const Ast& n, const std::shared_ptr<Env>& env) {
    switch (n.kind) {
        case Node::Number: return op_number(n);
        case Node::String: return Value(n.text);
        case Node::True: return Value(true);
        case Node::False: return Value(false);
        case Node::None: return Value();
        case Node::Name: {
            Value* v = env->find(n.text);
            if (!v) fail(n, "'" + n.text + "' is not defined");
            return *v;
        }
        case Node::Interp: {
            std::string s;
            for (const auto& part : n.kids) s += show(eval(*part, env));
            return Value(s);
        }
        case Node::List: {
            auto l = std::make_shared<List>();
            for (const auto& k : n.kids) l->push_back(eval(*k, env));
            return Value(l);
        }
        case Node::Map: {
            auto m = std::make_shared<Map>();
            for (const auto& p : n.kids) m->set(p->text, eval(*p->kids[0], env));
            return Value(m);
        }
        case Node::Range: {
            long long a = whole(*n.kids[0], eval(*n.kids[0], env), "range start");
            long long b = whole(*n.kids[1], eval(*n.kids[1], env), "range end");
            if (b - a > 10000000) fail(n, "range is too large");
            auto l = std::make_shared<List>();
            for (long long i = a; i < b; ++i) l->emplace_back(static_cast<long long>(i));
            return Value(l);
        }
        case Node::Unary: return op_unary(n, eval(*n.kids[0], env));
        case Node::Binary: return binary(n, env);
        case Node::Call: return call(n, env);
        case Node::Field: return op_field(n, eval(*n.kids[0], env));
        case Node::Index: {
            Value obj = eval(*n.kids[0], env);
            Value idx = eval(*n.kids[1], env);
            return op_index(n, obj, idx);
        }
        default:
            fail(n, std::string("cannot evaluate a ") + node_name(n.kind));
    }
}

Value Interpreter::binary(const Ast& n, const std::shared_ptr<Env>& env) {
    const std::string& op = n.text;
    if (op == "and" || op == "or") {
        bool left = condition(*n.kids[0], env, ("'" + op + "' operand").c_str());
        if (op == "and" && !left) return Value(false);
        if (op == "or" && left) return Value(true);
        return Value(condition(*n.kids[1], env, ("'" + op + "' operand").c_str()));
    }
    Value a = eval(*n.kids[0], env);
    Value b = eval(*n.kids[1], env);
    return op_binary(n, a, b);
}

Value Interpreter::call(const Ast& n, const std::shared_ptr<Env>& env) {
    Value callee = eval(*n.kids[0], env);
    std::vector<Value> args;
    for (size_t i = 1; i < n.kids.size(); ++i) args.push_back(eval(*n.kids[i], env));

    if (callee.is<std::shared_ptr<Builtin>>())
        return op_call_builtin(n, *callee.as<std::shared_ptr<Builtin>>(), args);
    if (!callee.is<std::shared_ptr<Function>>())
        fail(n, std::string("cannot call ") + a_type(callee));

    const Function& f = *callee.as<std::shared_ptr<Function>>();
    const Ast& params = *f.node->kids[0];
    if (args.size() != params.kids.size())
        fail(n, f.name + "() takes " + std::to_string(params.kids.size()) + " argument" +
                    (params.kids.size() == 1 ? "" : "s") + ", got " + std::to_string(args.size()));
    gc_maybe();
    if (depth_ >= kMaxDepth) fail(n, "too much recursion (more than " + std::to_string(kMaxDepth) + " calls deep)");

    auto scope = std::make_shared<Env>(f.closure);
    for (size_t i = 0; i < args.size(); ++i) {
        const Ast& prm = *params.kids[i];
        if (!prm.kids.empty()) {
            op_check_type(n, prm.kids[0]->text, args[i], f.name + "() argument '" + prm.text + "' must be");
            scope->hints[prm.text] = prm.kids[0]->text;
        }
        scope->vars[prm.text] = args[i];
    }
    const std::string& ret_hint = op_hint(*f.node);
    const std::string ret_what = f.name + "() must return";

    const Ast& body = *f.node->kids[1];
    ++depth_;
    struct Guard { int& d; ~Guard() { --d; } } guard{depth_};
    std::string saved = current_file_;
    current_file_ = f.file;  // so a `use` or `fun` inside resolves against the right file
    struct Restore { std::string& cur; std::string old; ~Restore() { cur = old; } } restore{current_file_, saved};
    Flow fl;
    try {
        if (body.kind != Node::Block) {  // short form: fun f(x) = expr
            Value r = eval(body, scope);
            if (!ret_hint.empty()) op_check_type(body, ret_hint, r, ret_what);
            return r;
        }
        fl = exec_block(body, scope);
        if (fl == Flow::Break) fail(*flow_node_, "'break' outside a loop");
        if (fl == Flow::Next) fail(*flow_node_, "'next' outside a loop");
        if (fl == Flow::Return) {
            Value r = std::move(ret_);
            ret_ = Value();
            if (!ret_hint.empty()) op_check_type(*flow_node_, ret_hint, r, ret_what);
            return r;
        }
        if (!ret_hint.empty()) op_check_type(*f.node, ret_hint, Value(), ret_what);
    } catch (RuntimeError& e) {
        if (e.file.empty() && f.file != saved) e.file = f.file;
        throw;
    }
    return Value();
}

}  // namespace relic
