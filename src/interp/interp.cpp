#include "interp/interp.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

namespace relic {

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

Interpreter::Interpreter(std::ostream& out)
    : out_(out), builtins_(std::make_shared<Env>()), globals_(std::make_shared<Env>(builtins_)) {
    install_builtins();
}

namespace fs = std::filesystem;

namespace {

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

}  // namespace

void Interpreter::set_main_file(const std::string& path) {
    current_file_ = path;
    if (path.empty() || path == "<repl>") return;
    // Register the main file, so a module that uses it back gets this same
    // scope instead of running the file a second time.
    std::string name = fs::path(path).stem().string();
    modules_[key_of(path)] = std::make_shared<Module>(Module{name, path, globals_});
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
    auto mod = std::make_shared<Module>(Module{name, file, std::make_shared<Env>(builtins_)});
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
        case Node::Let:
            env->vars[n.text] = eval(*n.kids[0], env);
            return Flow::Normal;
        case Node::Assign:
            assign(*n.kids[0], eval(*n.kids[1], env), env);
            return Flow::Normal;
        case Node::ExprStmt:
            eval(*n.kids[0], env);
            return Flow::Normal;
        case Node::Fun: {
            auto f = std::make_shared<Function>(Function{n.text, &n, env, current_file_});
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
                Flow f = exec_block(*n.kids[1], std::make_shared<Env>(env));
                if (f == Flow::Break) break;
                if (f == Flow::Return) return f;
            }
            return Flow::Normal;
        case Node::For: {
            Value src = eval(*n.kids[0], env);
            List items;
            if (src.is<std::shared_ptr<List>>()) {
                items = *src.as<std::shared_ptr<List>>();  // copy: changes in the loop do not affect it
            } else if (src.is<std::string>()) {
                for (char c : src.as<std::string>()) items.emplace_back(std::string(1, c));
            } else if (src.is<std::shared_ptr<Map>>()) {
                for (const auto& k : src.as<std::shared_ptr<Map>>()->keys) items.emplace_back(k);
            } else {
                fail(*n.kids[0], std::string("cannot loop over a ") + type_name(src));
            }
            for (auto& item : items) {
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
            env->vars[m.as<std::shared_ptr<Module>>()->name] = m;
            return Flow::Normal;
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

void Interpreter::assign(const Ast& target, Value v, const std::shared_ptr<Env>& env) {
    if (target.kind == Node::Name) {
        Value* slot = env->find(target.text);
        if (!slot) fail(target, "'" + target.text + "' is not declared; use 'let " + target.text + " = ...'");
        *slot = std::move(v);
        return;
    }
    Value obj = eval(*target.kids[0], env);
    if (target.kind == Node::Field) {
        if (obj.is<std::shared_ptr<Module>>())
            fail(target, "cannot change a module's names from outside; give it a function that does it");
        if (!obj.is<std::shared_ptr<Map>>())
            fail(target, std::string("cannot set a field on a ") + type_name(obj));
        obj.as<std::shared_ptr<Map>>()->set(target.text, std::move(v));
        return;
    }
    Value idx = eval(*target.kids[1], env);
    if (obj.is<std::shared_ptr<List>>()) {
        List& l = *obj.as<std::shared_ptr<List>>();
        l[list_slot(*target.kids[1], idx, l.size())] = std::move(v);
    } else if (obj.is<std::shared_ptr<Map>>()) {
        if (!idx.is<std::string>()) fail(*target.kids[1], "map keys must be strings");
        obj.as<std::shared_ptr<Map>>()->set(idx.as<std::string>(), std::move(v));
    } else {
        fail(target, std::string("cannot set an index on a ") + type_name(obj));
    }
}

Value Interpreter::eval(const Ast& n, const std::shared_ptr<Env>& env) {
    switch (n.kind) {
        case Node::Number: return Value(std::stod(n.text));
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
            for (long long i = a; i < b; ++i) l->emplace_back(static_cast<double>(i));
            return Value(l);
        }
        case Node::Unary: {
            Value v = eval(*n.kids[0], env);
            if (n.text == "-") {
                if (!v.is<double>()) fail(n, std::string("cannot negate a ") + type_name(v));
                return Value(-v.as<double>());
            }
            if (!v.is<bool>()) fail(n, std::string("'not' needs true or false, got ") + type_name(v));
            return Value(!v.as<bool>());
        }
        case Node::Binary: return binary(n, env);
        case Node::Call: return call(n, env);
        case Node::Field: {
            Value obj = eval(*n.kids[0], env);
            if (obj.is<std::shared_ptr<Module>>()) {
                const Module& m = *obj.as<std::shared_ptr<Module>>();
                auto it = m.scope->vars.find(n.text);
                if (it == m.scope->vars.end())
                    fail(n, "module '" + m.name + "' has no '" + n.text + "' (yet)");
                return it->second;
            }
            if (!obj.is<std::shared_ptr<Map>>())
                fail(n, std::string("cannot read a field of a ") + type_name(obj));
            const Map& m = *obj.as<std::shared_ptr<Map>>();
            if (!m.has(n.text)) fail(n, "map has no key '" + n.text + "'");
            return m.items.at(n.text);
        }
        case Node::Index: {
            Value obj = eval(*n.kids[0], env);
            Value idx = eval(*n.kids[1], env);
            if (obj.is<std::shared_ptr<List>>()) {
                const List& l = *obj.as<std::shared_ptr<List>>();
                return l[list_slot(*n.kids[1], idx, l.size())];
            }
            if (obj.is<std::string>()) {
                const std::string& s = obj.as<std::string>();
                return Value(std::string(1, s[list_slot(*n.kids[1], idx, s.size(), "string index")]));
            }
            if (obj.is<std::shared_ptr<Map>>()) {
                if (!idx.is<std::string>()) fail(*n.kids[1], "map keys must be strings");
                const Map& m = *obj.as<std::shared_ptr<Map>>();
                const std::string& k = idx.as<std::string>();
                if (!m.has(k)) fail(*n.kids[1], "map has no key '" + k + "'");
                return m.items.at(k);
            }
            fail(n, std::string("cannot index a ") + type_name(obj));
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
    auto mismatch = [&](const char* verb) -> Value {
        fail(n, std::string("cannot ") + verb + " a " + type_name(a) + " and a " + type_name(b));
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
        fail(n, std::string("cannot look inside a ") + type_name(b));
    }
    if (op == "+") {
        if (a.is<double>() && b.is<double>()) return Value(a.as<double>() + b.as<double>());
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
        if (a.is<double>() && b.is<double>()) {
            double x = a.as<double>(), y = b.as<double>();
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
    if (!a.is<double>() || !b.is<double>()) {
        if (op == "-") return mismatch("subtract");
        if (op == "*") return mismatch("multiply");
        return mismatch("divide");
    }
    double x = a.as<double>(), y = b.as<double>();
    if (op == "-") return Value(x - y);
    if (op == "*") return Value(x * y);
    if (y == 0) fail(n, "division by zero");
    if (op == "/") return Value(x / y);
    double r = std::fmod(x, y);
    if (r != 0 && ((r < 0) != (y < 0))) r += y;  // result takes the sign of the divisor
    return Value(r);
}

Value Interpreter::call(const Ast& n, const std::shared_ptr<Env>& env) {
    Value callee = eval(*n.kids[0], env);
    std::vector<Value> args;
    for (size_t i = 1; i < n.kids.size(); ++i) args.push_back(eval(*n.kids[i], env));

    if (callee.is<std::shared_ptr<Builtin>>()) {
        const Builtin& b = *callee.as<std::shared_ptr<Builtin>>();
        if (b.arity >= 0 && static_cast<int>(args.size()) != b.arity)
            fail(n, b.name + "() takes " + std::to_string(b.arity) + " argument" +
                        (b.arity == 1 ? "" : "s") + ", got " + std::to_string(args.size()));
        try {
            return b.fn(args);
        } catch (const std::string& msg) {
            fail(n, b.name + "(): " + msg);
        }
    }
    if (!callee.is<std::shared_ptr<Function>>())
        fail(n, std::string("cannot call a ") + type_name(callee));

    const Function& f = *callee.as<std::shared_ptr<Function>>();
    const Ast& params = *f.node->kids[0];
    if (args.size() != params.kids.size())
        fail(n, f.name + "() takes " + std::to_string(params.kids.size()) + " argument" +
                    (params.kids.size() == 1 ? "" : "s") + ", got " + std::to_string(args.size()));
    if (depth_ >= kMaxDepth) fail(n, "too much recursion (more than " + std::to_string(kMaxDepth) + " calls deep)");

    auto scope = std::make_shared<Env>(f.closure);
    for (size_t i = 0; i < args.size(); ++i) scope->vars[params.kids[i]->text] = args[i];

    const Ast& body = *f.node->kids[1];
    ++depth_;
    struct Guard { int& d; ~Guard() { --d; } } guard{depth_};
    std::string saved = current_file_;
    current_file_ = f.file;  // so a `use` or `fun` inside resolves against the right file
    struct Restore { std::string& cur; std::string old; ~Restore() { cur = old; } } restore{current_file_, saved};
    Flow fl;
    try {
        if (body.kind != Node::Block) return eval(body, scope);  // short form: fun f(x) = expr
        fl = exec_block(body, scope);
    } catch (RuntimeError& e) {
        if (e.file.empty() && f.file != saved) e.file = f.file;
        throw;
    }
    if (fl == Flow::Break) fail(*flow_node_, "'break' outside a loop");
    if (fl == Flow::Next) fail(*flow_node_, "'next' outside a loop");
    if (fl == Flow::Return) {
        Value r = std::move(ret_);
        ret_ = Value();
        return r;
    }
    return Value();
}

}  // namespace relic
