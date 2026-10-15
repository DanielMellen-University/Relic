#include "vm/vm.hpp"

#include "interp/interp.hpp"
#include "interp/ops.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace relic {

namespace {

enum class Op {
    Const, None, True, False,
    Load, Store, LoadGlobal, StoreGlobal, DefineGlobal, LoadBuiltin,
    Pop, EnterScope, ExitScope,
    Jump, JumpIfFalse, CheckBool, JumpIfFalseKeep, JumpIfTrueKeep,
    Binary, Unary, Range, Field, Index, SetField, SetIndex,
    List, Map, Interp, Call, Closure, Return, ReturnNone,
    ForPrep, ForNext, Fail,
};

const char* op_name(Op o) {
    static const char* names[] = {
        "CONST", "NONE", "TRUE", "FALSE",
        "LOAD", "STORE", "LOAD_GLOBAL", "STORE_GLOBAL", "DEFINE_GLOBAL", "LOAD_BUILTIN",
        "POP", "ENTER_SCOPE", "EXIT_SCOPE",
        "JUMP", "JUMP_IF_FALSE", "CHECK_BOOL", "JUMP_IF_FALSE_KEEP", "JUMP_IF_TRUE_KEEP",
        "BINARY", "UNARY", "RANGE", "FIELD", "INDEX", "SET_FIELD", "SET_INDEX",
        "LIST", "MAP", "INTERP", "CALL", "CLOSURE", "RETURN", "RETURN_NONE",
        "FOR_PREP", "FOR_NEXT", "FAIL",
    };
    return names[static_cast<int>(o)];
}

struct Instr {
    Op op;
    int a = 0;
    int b = 0;
    const Ast* at = nullptr;  // where errors point
};

struct Proto {
    std::string name;
    const Ast* node = nullptr;
    int nparams = 0;
    int nslots = 0;  // slots of the function's own scope (params first)
    std::vector<Instr> code;
    std::vector<Value> consts;
    std::vector<std::string> strings;  // messages and names
};

struct Scope {
    std::vector<Value> slots;
    std::shared_ptr<Scope> parent;
    Scope(int n, std::shared_ptr<Scope> p) : slots(static_cast<size_t>(n)), parent(std::move(p)) {}
};

struct Program {
    std::vector<std::unique_ptr<Proto>> protos;  // protos[0] is the main program
    std::unordered_map<std::string, int> global_index;
    std::vector<std::string> global_names;
};

// ---- compiler ----

class Compiler {
public:
    explicit Compiler(Program& prog) : prog_(prog) {}

    void compile_main(const Ast& program) {
        for (const auto& s : program.kids)
            if (s->kind == Node::Let || s->kind == Node::Fun) global(s->text);
        auto p = std::make_unique<Proto>();
        p->name = "<main>";
        p->node = &program;
        Proto* main = p.get();
        prog_.protos.push_back(std::move(p));
        FnState fs{main, scopes_.size(), {}};
        fns_.push_back(fs);
        for (const auto& s : program.kids) stmt(*s);
        emit(Op::ReturnNone);
        fns_.pop_back();
    }

private:
    struct CScope {
        std::unordered_map<std::string, int> names;
        int count = 0;
        size_t enter_ip = 0;     // ENTER_SCOPE to patch with the slot count
        bool fn_scope = false;   // a function's own scope: its Proto holds the count
    };
    struct Loop {
        size_t scope_depth;      // scopes_.size() outside the loop
        int stack_extra;         // values the loop keeps on the stack (for: list and index)
        size_t continue_ip;
        std::vector<size_t> breaks;
        std::vector<size_t> nexts;  // jumps patched to continue_ip when known
    };
    struct FnState {
        Proto* proto;
        size_t scope_base;       // scopes_ index where this function's scopes start
        std::vector<Loop> loops;
    };

    Program& prog_;
    std::vector<CScope> scopes_;
    std::vector<FnState> fns_;

    Proto& cur() { return *fns_.back().proto; }
    size_t emit(Op op, int a = 0, int b = 0, const Ast* at = nullptr) {
        cur().code.push_back({op, a, b, at});
        return cur().code.size() - 1;
    }
    size_t here() { return cur().code.size(); }
    void patch(size_t ip, size_t target) { cur().code[ip].a = static_cast<int>(target); }
    int str(const std::string& s) {
        cur().strings.push_back(s);
        return static_cast<int>(cur().strings.size() - 1);
    }
    int constant(Value v) {
        cur().consts.push_back(std::move(v));
        return static_cast<int>(cur().consts.size() - 1);
    }
    int global(const std::string& name) {
        auto it = prog_.global_index.find(name);
        if (it != prog_.global_index.end()) return it->second;
        int i = static_cast<int>(prog_.global_names.size());
        prog_.global_index[name] = i;
        prog_.global_names.push_back(name);
        return i;
    }

    static bool declares(const Ast& block) {
        for (const auto& s : block.kids)
            if (s->kind == Node::Let || s->kind == Node::Fun) return true;
        return false;
    }

    // Scopes: only blocks that declare names get one at run time.
    void open_scope() {
        CScope s;
        s.enter_ip = emit(Op::EnterScope);
        scopes_.push_back(std::move(s));
    }
    void close_scope() {
        cur().code[scopes_.back().enter_ip].a = scopes_.back().count;
        scopes_.pop_back();
        emit(Op::ExitScope, 1);
    }

    // Declares in the innermost scope, or as a global at the top level.
    // Emits the store of the value on the stack.
    void declare(const std::string& name, const Ast& at) {
        if (scopes_.empty()) {
            emit(Op::DefineGlobal, global(name), 0, &at);
            return;
        }
        CScope& s = scopes_.back();
        auto it = s.names.find(name);
        int slot;
        if (it != s.names.end()) {
            slot = it->second;
        } else {
            slot = s.count++;
            s.names[name] = slot;
            if (s.fn_scope) cur().nslots = s.count;
        }
        emit(Op::Store, 0, slot, &at);
    }

    bool resolve(const std::string& name, int& hops, int& slot) {
        hops = 0;
        for (size_t i = scopes_.size(); i-- > 0;) {
            auto it = scopes_[i].names.find(name);
            if (it != scopes_[i].names.end()) {
                slot = it->second;
                return true;
            }
            ++hops;
        }
        return false;
    }

    void load_name(const Ast& n) {
        int hops, slot;
        if (resolve(n.text, hops, slot)) {
            emit(Op::Load, hops, slot, &n);
        } else if (prog_.global_index.count(n.text)) {
            emit(Op::LoadGlobal, prog_.global_index[n.text], str(n.text), &n);
        } else {
            emit(Op::LoadBuiltin, str(n.text), 0, &n);
        }
    }

    // Leaves the loop: drops scopes opened inside it and its stack values.
    void unwind_to(const Loop& loop, bool drop_stack) {
        size_t extra = scopes_.size() - loop.scope_depth;
        if (extra) emit(Op::ExitScope, static_cast<int>(extra));
        if (drop_stack)
            for (int i = 0; i < loop.stack_extra; ++i) emit(Op::Pop);
    }

    void block(const Ast& b, bool new_scope) {
        bool scoped = new_scope && declares(b);
        if (scoped) open_scope();
        for (const auto& s : b.kids) stmt(*s);
        if (scoped) close_scope();
    }

    void stmt(const Ast& n) {
        switch (n.kind) {
            case Node::Let:
                expr(*n.kids[0]);
                declare(n.text, n);
                return;
            case Node::Fun: {
                // Declare first so the body can call itself.
                emit(Op::None);
                declare(n.text, n);
                cur().code.pop_back();
                cur().code.pop_back();
                int idx = function(n);
                emit(Op::Closure, idx, 0, &n);
                declare(n.text, n);
                return;
            }
            case Node::Assign: {
                const Ast& t = *n.kids[0];
                expr(*n.kids[1]);
                if (t.kind == Node::Name) {
                    int hops, slot;
                    if (resolve(t.text, hops, slot)) emit(Op::Store, hops, slot, &t);
                    else emit(Op::StoreGlobal, global_or_new(t.text), str(t.text), &t);
                } else if (t.kind == Node::Field) {
                    expr(*t.kids[0]);
                    emit(Op::SetField, 0, 0, &t);
                } else {
                    expr(*t.kids[0]);
                    expr(*t.kids[1]);
                    emit(Op::SetIndex, 0, 0, &t);
                }
                return;
            }
            case Node::ExprStmt:
                expr(*n.kids[0]);
                emit(Op::Pop);
                return;
            case Node::If: {
                expr(*n.kids[0]);
                size_t jf = emit(Op::JumpIfFalse, 0, str("'if' condition"), n.kids[0].get());
                block(*n.kids[1], true);
                if (n.kids.size() > 2) {
                    size_t jend = emit(Op::Jump);
                    patch(jf, here());
                    const Ast& alt = *n.kids[2];
                    if (alt.kind == Node::If) stmt(alt);
                    else block(alt, true);
                    patch(jend, here());
                } else {
                    patch(jf, here());
                }
                return;
            }
            case Node::While: {
                size_t top = here();
                expr(*n.kids[0]);
                size_t jf = emit(Op::JumpIfFalse, 0, str("'while' condition"), n.kids[0].get());
                fns_.back().loops.push_back({scopes_.size(), 0, top, {}, {}});
                block(*n.kids[1], true);
                emit(Op::Jump, static_cast<int>(top));
                Loop loop = std::move(fns_.back().loops.back());
                fns_.back().loops.pop_back();
                for (size_t ip : loop.nexts) patch(ip, top);
                patch(jf, here());
                for (size_t ip : loop.breaks) patch(ip, here());
                return;
            }
            case Node::For: {
                expr(*n.kids[0]);
                emit(Op::ForPrep, 0, 0, n.kids[0].get());
                size_t top = here();
                size_t fn = emit(Op::ForNext);
                fns_.back().loops.push_back({scopes_.size(), 2, top, {}, {}});
                // Each turn gets a scope holding the loop name and the body's names.
                open_scope();
                declare(n.text, n);
                for (const auto& s : n.kids[1]->kids) stmt(*s);
                Loop loop = std::move(fns_.back().loops.back());
                fns_.back().loops.pop_back();
                size_t next_target = here();
                close_scope();
                emit(Op::Jump, static_cast<int>(top));
                for (size_t ip : loop.nexts) patch(ip, next_target);
                patch(fn, here());
                for (size_t ip : loop.breaks) patch(ip, here());
                return;
            }
            case Node::Return:
                if (n.kids.empty()) emit(Op::None);
                else expr(*n.kids[0]);
                if (fns_.size() == 1) {
                    emit(Op::Fail, str("'return' outside a function"), 0, &n);
                    return;
                }
                emit(Op::Return);
                return;
            case Node::Break:
            case Node::Next: {
                bool is_break = n.kind == Node::Break;
                auto& loops = fns_.back().loops;
                if (loops.empty()) {
                    emit(Op::Fail, str(is_break ? "'break' outside a loop" : "'next' outside a loop"), 0, &n);
                    return;
                }
                Loop& loop = loops.back();
                if (is_break) {
                    unwind_to(loop, true);
                    loop.breaks.push_back(emit(Op::Jump));
                } else {
                    // For loops: leave the turn's scope except the one closed at next_target.
                    size_t keep = loop.stack_extra ? 1 : 0;
                    size_t extra = scopes_.size() - loop.scope_depth - keep;
                    if (extra) emit(Op::ExitScope, static_cast<int>(extra));
                    loop.nexts.push_back(emit(Op::Jump));
                }
                return;
            }
            case Node::Use:
                emit(Op::Fail, str("'use' is not supported by the VM yet; run without --vm"), 0, &n);
                return;
            default:
                emit(Op::Fail, str(std::string("cannot run a ") + node_name(n.kind) + " here"), 0, &n);
        }
    }

    int global_or_new(const std::string& name) {
        auto it = prog_.global_index.find(name);
        return it == prog_.global_index.end() ? -1 : it->second;
    }

    int function(const Ast& n) {
        auto p = std::make_unique<Proto>();
        p->name = n.text;
        p->node = &n;
        const Ast& params = *n.kids[0];
        p->nparams = static_cast<int>(params.kids.size());
        Proto* proto = p.get();
        int idx = static_cast<int>(prog_.protos.size());
        prog_.protos.push_back(std::move(p));

        fns_.push_back({proto, scopes_.size(), {}});
        CScope s;
        s.fn_scope = true;
        for (const auto& prm : params.kids) s.names[prm->text] = s.count++;
        proto->nslots = s.count;
        scopes_.push_back(std::move(s));
        const Ast& body = *n.kids[1];
        if (body.kind == Node::Block) {
            for (const auto& st : body.kids) stmt(*st);
            emit(Op::ReturnNone);
        } else {
            expr(body);
            emit(Op::Return);
        }
        scopes_.pop_back();
        fns_.pop_back();
        return idx;
    }

    void expr(const Ast& n) {
        switch (n.kind) {
            case Node::Number: emit(Op::Const, constant(Value(std::stod(n.text)))); return;
            case Node::String: emit(Op::Const, constant(Value(n.text))); return;
            case Node::True: emit(Op::True); return;
            case Node::False: emit(Op::False); return;
            case Node::None: emit(Op::None); return;
            case Node::Name: load_name(n); return;
            case Node::Interp:
                for (const auto& k : n.kids) expr(*k);
                emit(Op::Interp, static_cast<int>(n.kids.size()));
                return;
            case Node::List:
                for (const auto& k : n.kids) expr(*k);
                emit(Op::List, static_cast<int>(n.kids.size()));
                return;
            case Node::Map:
                for (const auto& p : n.kids) expr(*p->kids[0]);
                emit(Op::Map, static_cast<int>(n.kids.size()), 0, &n);
                return;
            case Node::Range:
                expr(*n.kids[0]);
                expr(*n.kids[1]);
                emit(Op::Range, 0, 0, &n);
                return;
            case Node::Unary:
                expr(*n.kids[0]);
                emit(Op::Unary, 0, 0, &n);
                return;
            case Node::Binary:
                if (n.text == "and" || n.text == "or") {
                    int msg = str("'" + n.text + "' operand");
                    expr(*n.kids[0]);
                    emit(Op::CheckBool, 0, msg, n.kids[0].get());
                    size_t j = emit(n.text == "and" ? Op::JumpIfFalseKeep : Op::JumpIfTrueKeep);
                    emit(Op::Pop);
                    expr(*n.kids[1]);
                    emit(Op::CheckBool, 0, msg, n.kids[1].get());
                    patch(j, here());
                    return;
                }
                expr(*n.kids[0]);
                expr(*n.kids[1]);
                emit(Op::Binary, 0, 0, &n);
                return;
            case Node::Call:
                for (const auto& k : n.kids) expr(*k);
                emit(Op::Call, static_cast<int>(n.kids.size() - 1), 0, &n);
                return;
            case Node::Field:
                expr(*n.kids[0]);
                emit(Op::Field, 0, 0, &n);
                return;
            case Node::Index:
                expr(*n.kids[0]);
                expr(*n.kids[1]);
                emit(Op::Index, 0, 0, &n);
                return;
            default:
                emit(Op::Fail, str(std::string("cannot evaluate a ") + node_name(n.kind)), 0, &n);
        }
    }
};

// ---- machine ----

class Machine {
public:
    Machine(Program& prog, std::ostream& out)
        : prog_(prog), host_(out), globals_(prog.global_names.size()),
          defined_(prog.global_names.size(), 0) {}

    void run_main() { run(*prog_.protos[0], nullptr); }

private:
    Program& prog_;
    Interpreter host_;  // owns the built-ins (print writes to its stream)
    std::vector<Value> globals_;
    std::vector<char> defined_;
    int depth_ = 0;

    const Value* builtin(const std::string& name) {
        auto it = host_.builtins()->vars.find(name);
        return it == host_.builtins()->vars.end() ? nullptr : &it->second;
    }

    Value call(const Instr& in, Value callee, std::vector<Value>& args) {
        if (callee.is<std::shared_ptr<Builtin>>())
            return op_call_builtin(*in.at, *callee.as<std::shared_ptr<Builtin>>(), args);
        if (!callee.is<std::shared_ptr<Function>>())
            fail_at(*in.at, std::string("cannot call a ") + type_name(callee));
        const Function& f = *callee.as<std::shared_ptr<Function>>();
        if (!f.proto) fail_at(*in.at, "cannot call a function made outside the VM");
        const Proto& p = *static_cast<const Proto*>(f.proto);
        if (args.size() != static_cast<size_t>(p.nparams))
            fail_at(*in.at, op_arity_message(f.name, static_cast<size_t>(p.nparams), args.size()));
        if (depth_ >= kMaxCallDepth)
            fail_at(*in.at, "too much recursion (more than " + std::to_string(kMaxCallDepth) + " calls deep)");
        auto scope = std::make_shared<Scope>(p.nslots, std::static_pointer_cast<Scope>(f.vm_scope));
        for (size_t i = 0; i < args.size(); ++i) scope->slots[i] = std::move(args[i]);
        ++depth_;
        struct Guard { int& d; ~Guard() { --d; } } guard{depth_};
        return run(p, std::move(scope));
    }

    Value run(const Proto& p, std::shared_ptr<Scope> sc) {
        std::vector<Value> st;
        st.reserve(16);
        auto pop = [&]() {
            Value v = std::move(st.back());
            st.pop_back();
            return v;
        };
        size_t ip = 0;
        const size_t n = p.code.size();
        while (ip < n) {
            const Instr& in = p.code[ip++];
            switch (in.op) {
                case Op::Const: st.push_back(p.consts[in.a]); break;
                case Op::None: st.emplace_back(); break;
                case Op::True: st.emplace_back(true); break;
                case Op::False: st.emplace_back(false); break;
                case Op::Load: {
                    Scope* s = sc.get();
                    for (int h = 0; h < in.a; ++h) s = s->parent.get();
                    st.push_back(s->slots[in.b]);
                    break;
                }
                case Op::Store: {
                    Scope* s = sc.get();
                    for (int h = 0; h < in.a; ++h) s = s->parent.get();
                    s->slots[in.b] = pop();
                    break;
                }
                case Op::LoadGlobal:
                    if (defined_[in.a]) {
                        st.push_back(globals_[in.a]);
                        break;
                    }
                    [[fallthrough]];
                case Op::LoadBuiltin: {
                    const std::string& name = p.strings[in.op == Op::LoadGlobal ? in.b : in.a];
                    const Value* b = builtin(name);
                    if (!b) fail_at(*in.at, "'" + name + "' is not defined");
                    st.push_back(*b);
                    break;
                }
                case Op::DefineGlobal:
                    globals_[in.a] = pop();
                    defined_[in.a] = 1;
                    break;
                case Op::StoreGlobal: {
                    const std::string& name = p.strings[in.b];
                    if (in.a < 0 || !defined_[in.a])
                        fail_at(*in.at, "'" + name + "' is not declared; use 'let " + name + " = ...'");
                    globals_[in.a] = pop();
                    break;
                }
                case Op::Pop: st.pop_back(); break;
                case Op::EnterScope: sc = std::make_shared<Scope>(in.a, std::move(sc)); break;
                case Op::ExitScope:
                    for (int i = 0; i < in.a; ++i) sc = sc->parent;
                    break;
                case Op::Jump: ip = static_cast<size_t>(in.a); break;
                case Op::JumpIfFalse:
                    if (!op_condition(*in.at, st.back(), p.strings[in.b].c_str())) ip = static_cast<size_t>(in.a);
                    st.pop_back();
                    break;
                case Op::CheckBool: op_condition(*in.at, st.back(), p.strings[in.b].c_str()); break;
                case Op::JumpIfFalseKeep:
                    if (!st.back().as<bool>()) ip = static_cast<size_t>(in.a);
                    break;
                case Op::JumpIfTrueKeep:
                    if (st.back().as<bool>()) ip = static_cast<size_t>(in.a);
                    break;
                case Op::Binary: {
                    Value b = pop();
                    Value& a = st.back();
                    // Fast path for number math, the hot case in loops.
                    if (a.is<double>() && b.is<double>()) {
                        const std::string& o = in.at->text;
                        double x = a.as<double>(), y = b.as<double>();
                        if (o.size() == 1) {
                            if (o[0] == '+') { a = Value(x + y); break; }
                            if (o[0] == '-') { a = Value(x - y); break; }
                            if (o[0] == '*') { a = Value(x * y); break; }
                            if (o[0] == '<') { a = Value(x < y); break; }
                            if (o[0] == '>') { a = Value(x > y); break; }
                        }
                    }
                    a = op_binary(*in.at, a, b);
                    break;
                }
                case Op::Unary: st.back() = op_unary(*in.at, st.back()); break;
                case Op::Range: {
                    Value b = pop();
                    st.back() = op_range(*in.at, st.back(), b);
                    break;
                }
                case Op::Field: st.back() = op_field(*in.at, st.back()); break;
                case Op::Index: {
                    Value idx = pop();
                    st.back() = op_index(*in.at, st.back(), idx);
                    break;
                }
                case Op::SetField: {
                    Value obj = pop();
                    Value v = pop();
                    op_set_field(*in.at, obj, std::move(v));
                    break;
                }
                case Op::SetIndex: {
                    Value idx = pop();
                    Value obj = pop();
                    Value v = pop();
                    op_set_index(*in.at, obj, idx, std::move(v));
                    break;
                }
                case Op::List: {
                    auto l = std::make_shared<List>(std::make_move_iterator(st.end() - in.a),
                                                    std::make_move_iterator(st.end()));
                    st.resize(st.size() - static_cast<size_t>(in.a));
                    st.emplace_back(l);
                    break;
                }
                case Op::Map: {
                    auto m = std::make_shared<Map>();
                    size_t base = st.size() - static_cast<size_t>(in.a);
                    for (int i = 0; i < in.a; ++i) m->set(in.at->kids[i]->text, std::move(st[base + i]));
                    st.resize(base);
                    st.emplace_back(m);
                    break;
                }
                case Op::Interp: {
                    std::string s;
                    size_t base = st.size() - static_cast<size_t>(in.a);
                    for (size_t i = base; i < st.size(); ++i) s += show(st[i]);
                    st.resize(base);
                    st.emplace_back(std::move(s));
                    break;
                }
                case Op::Call: {
                    size_t base = st.size() - static_cast<size_t>(in.a);
                    std::vector<Value> args(std::make_move_iterator(st.begin() + base),
                                            std::make_move_iterator(st.end()));
                    st.resize(base);
                    Value callee = pop();
                    st.push_back(call(in, std::move(callee), args));
                    break;
                }
                case Op::Closure: {
                    const Proto& fp = *prog_.protos[in.a];
                    auto f = std::make_shared<Function>(Function{fp.name, fp.node, nullptr, &fp, sc, std::string()});
                    f->proto = &fp;
                    f->vm_scope = sc;
                    st.emplace_back(f);
                    break;
                }
                case Op::Return: return pop();
                case Op::ReturnNone: return Value();
                case Op::ForPrep: {
                    Value src = pop();
                    st.emplace_back(std::make_shared<List>(op_loop_items(*in.at, src)));
                    st.emplace_back(0.0);
                    break;
                }
                case Op::ForNext: {
                    double& i = std::get<double>(st.back().v);
                    const List& items = *st[st.size() - 2].as<std::shared_ptr<List>>();
                    if (static_cast<size_t>(i) < items.size()) {
                        Value item = items[static_cast<size_t>(i)];
                        i += 1;
                        st.push_back(std::move(item));
                    } else {
                        st.pop_back();
                        st.pop_back();
                        ip = static_cast<size_t>(in.a);
                    }
                    break;
                }
                case Op::Fail: fail_at(*in.at, p.strings[in.a]);
            }
        }
        return Value();
    }
};

}  // namespace

void vm_run(const Ast& program, std::ostream& out) {
    Program prog;
    Compiler(prog).compile_main(program);
    Machine(prog, out).run_main();
}

std::string vm_disassemble(const Ast& program) {
    Program prog;
    Compiler(prog).compile_main(program);
    std::ostringstream o;
    for (const auto& p : prog.protos) {
        o << "== " << p->name << " (params " << p->nparams << ", slots " << p->nslots << ")\n";
        for (size_t i = 0; i < p->code.size(); ++i) {
            const Instr& in = p->code[i];
            o << i << " " << op_name(in.op) << " " << in.a << " " << in.b;
            if (in.op == Op::Const) o << " ; " << repr(p->consts[in.a]);
            o << "\n";
        }
    }
    return o.str();
}

}  // namespace relic
