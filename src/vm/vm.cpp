#include "vm/vm.hpp"
#include "diag/diag.hpp"

#include "interp/interp.hpp"
#include "interp/ops.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <algorithm>
#include <fstream>

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
    ForPrep, ForNext, ForRangePrep, ForRangeNext, Fail, Use, TryBegin, TryEnd, CheckType,
};

const char* op_name(Op o) {
    static const char* names[] = {
        "CONST", "NONE", "TRUE", "FALSE",
        "LOAD", "STORE", "LOAD_GLOBAL", "STORE_GLOBAL", "DEFINE_GLOBAL", "LOAD_BUILTIN",
        "POP", "ENTER_SCOPE", "EXIT_SCOPE",
        "JUMP", "JUMP_IF_FALSE", "CHECK_BOOL", "JUMP_IF_FALSE_KEEP", "JUMP_IF_TRUE_KEEP",
        "BINARY", "UNARY", "RANGE", "FIELD", "INDEX", "SET_FIELD", "SET_INDEX",
        "LIST", "MAP", "INTERP", "CALL", "CLOSURE", "RETURN", "RETURN_NONE",
        "FOR_PREP", "FOR_NEXT", "FOR_RANGE_PREP", "FOR_RANGE_NEXT", "FAIL", "USE", "TRY_BEGIN", "TRY_END", "CHECK_TYPE",
    };
    return names[static_cast<int>(o)];
}

// Binary operators by number, so the VM's fast paths skip string compares.
enum BinOp { BOther, BAdd, BSub, BMul, BDiv, BMod, BEq, BNe, BLt, BLe, BGt, BGe };
int binop_code(const std::string& o) {
    static const char* names[] = {"", "+", "-", "*", "/", "%", "==", "!=", "<", "<=", ">", ">="};
    for (int i = 1; i < 12; ++i)
        if (o == names[i]) return i;
    return BOther;
}

struct Instr {
    Op op;
    int a = 0;
    int b = 0;
    const Ast* at = nullptr;  // where errors point
};

struct Program;

struct Proto {
    Program* mod = nullptr;  // module whose globals this code uses
    std::string name;
    const Ast* node = nullptr;
    int nparams = 0;
    int nslots = 0;  // slots of the function's own scope (params first)
    std::vector<Instr> code;
    std::vector<Value> consts;
    std::vector<std::string> strings;  // messages and names
    std::vector<std::string> param_hints;  // `as` type per parameter, "" for none
};

struct Scope : GcObj {
    std::vector<Value> slots;
    std::shared_ptr<Scope> parent;
    Scope(int n, std::shared_ptr<Scope> p) : slots(static_cast<size_t>(n)), parent(std::move(p)) {}
    void trace(std::vector<GcObj*>& out) const override {
        for (const Value& v : slots) trace_value(v, out);
        if (parent) out.push_back(parent.get());
    }
    void clear_refs() override {
        std::vector<Value> gone;
        gone.swap(slots);
        auto p = std::move(parent);
    }
};

// One compiled file: the main program or a module.
struct Program {
    std::string file;
    std::unique_ptr<Ast> tree;  // modules own their tree; the main tree belongs to the caller
    std::vector<std::unique_ptr<Proto>> protos;  // protos[0] is the file's top level
    std::unordered_map<std::string, int> global_index;
    std::vector<std::string> global_names;
    std::vector<Value> globals;
    std::vector<char> defined;
    std::shared_ptr<Module> value;

    const Value* lookup(const std::string& name) const {
        auto it = global_index.find(name);
        if (it == global_index.end() || !defined[it->second]) return nullptr;
        return &globals[it->second];
    }
};

// ---- compiler ----

class Compiler {
public:
    explicit Compiler(Program& prog) : prog_(prog) {}

    void compile_main(const Ast& program) {
        for (const auto& s : program.kids) {
            if (s->kind == Node::Let || s->kind == Node::Fun) global(s->text);
            if (s->kind == Node::Let && !op_hint(*s).empty()) global_hints_[s->text] = op_hint(*s);
        }
        auto p = std::make_unique<Proto>();
        p->name = "<main>";
        p->mod = &prog_;
        p->node = &program;
        Proto* main = p.get();
        prog_.protos.push_back(std::move(p));
        FnState fs{main, scopes_.size(), {}};
        fns_.push_back(fs);
        for (const auto& s : program.kids) stmt(*s);
        emit(Op::ReturnNone);
        fns_.pop_back();
        prog_.globals.resize(prog_.global_names.size());
        prog_.defined.assign(prog_.global_names.size(), 0);
    }

private:
    struct CScope {
        std::unordered_map<std::string, int> names;
        std::unordered_map<std::string, std::string> hints;  // `as` types
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
        int try_depth = 0;          // open `try` blocks outside the loop
    };
    struct FnState {
        Proto* proto;
        size_t scope_base;       // scopes_ index where this function's scopes start
        std::vector<Loop> loops;
        int try_depth = 0;       // open `try` blocks in this function
    };

    Program& prog_;
    std::unordered_map<std::string, std::string> global_hints_;
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
            // b: the local names in scope here, for a "did you mean" hint.
            std::string locals;
            for (const auto& sc : scopes_)
                for (const auto& kv : sc.names) locals += kv.first + "\n";
            emit(Op::LoadBuiltin, str(n.text), str(locals), &n);
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
            case Node::Let: {
                expr(*n.kids[0]);
                const std::string& hint = op_hint(n);
                if (!hint.empty())
                    emit(Op::CheckType, str(hint), str("'" + n.text + "' must be"), n.kids[0].get());
                if (!scopes_.empty()) {
                    if (hint.empty()) scopes_.back().hints.erase(n.text);
                    else scopes_.back().hints[n.text] = hint;
                }
                declare(n.text, n);
                return;
            }
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
                    bool local = resolve(t.text, hops, slot);
                    const auto& hints = local ? scopes_[scopes_.size() - 1 - hops].hints : global_hints_;
                    auto h = hints.find(t.text);
                    if (h != hints.end())
                        emit(Op::CheckType, str(h->second), str("'" + t.text + "' must be"), n.kids[1].get());
                    if (local) emit(Op::Store, hops, slot, &t);
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
                fns_.back().loops.push_back({scopes_.size(), 0, top, {}, {}, fns_.back().try_depth});
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
                // `for i in a..b` counts on the stack instead of building the list.
                bool range = n.kids[0]->kind == Node::Range;
                if (range) {
                    expr(*n.kids[0]->kids[0]);
                    expr(*n.kids[0]->kids[1]);
                    emit(Op::ForRangePrep, 0, 0, n.kids[0].get());
                } else {
                    expr(*n.kids[0]);
                    emit(Op::ForPrep, 0, 0, n.kids[0].get());
                }
                size_t top = here();
                size_t fn = emit(range ? Op::ForRangeNext : Op::ForNext);
                fns_.back().loops.push_back({scopes_.size(), 2, top, {}, {}, fns_.back().try_depth});
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
                check_return(n);
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
                for (int i = loop.try_depth; i < fns_.back().try_depth; ++i) emit(Op::TryEnd);
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
            case Node::Try: {
                size_t begin = emit(Op::TryBegin, 0, 0, &n);
                ++fns_.back().try_depth;
                block(*n.kids[0], true);
                --fns_.back().try_depth;
                emit(Op::TryEnd);
                size_t jend = emit(Op::Jump);
                patch(begin, here());
                // The error value is on the stack; the catch block gets its own scope.
                if (n.text.empty()) {
                    emit(Op::Pop);
                    block(*n.kids[1], true);
                } else {
                    open_scope();
                    declare(n.text, n);
                    for (const auto& s : n.kids[1]->kids) stmt(*s);
                    close_scope();
                }
                patch(jend, here());
                return;
            }
            case Node::Use:
                emit(Op::Use, str(n.text), 0, &n);
                declare(n.kids.empty() ? n.text.substr(n.text.find_last_of('/') + 1) : n.kids[0]->text, n);
                return;
            default:
                emit(Op::Fail, str(std::string("cannot run a ") + node_name(n.kind) + " here"), 0, &n);
        }
    }

    // Checks the value on the stack against the current function's `as` return type.
    void check_return(const Ast& at) {
        const std::string& hint = op_hint(*cur().node);
        if (!hint.empty()) emit(Op::CheckType, str(hint), str(cur().name + "() must return"), &at);
    }

    int global_or_new(const std::string& name) {
        auto it = prog_.global_index.find(name);
        return it == prog_.global_index.end() ? -1 : it->second;
    }

    int function(const Ast& n) {
        auto p = std::make_unique<Proto>();
        p->name = n.text;
        p->node = &n;
        p->mod = &prog_;
        const Ast& params = *n.kids[0];
        p->nparams = static_cast<int>(params.kids.size());
        Proto* proto = p.get();
        int idx = static_cast<int>(prog_.protos.size());
        prog_.protos.push_back(std::move(p));

        fns_.push_back({proto, scopes_.size(), {}});
        CScope s;
        s.fn_scope = true;
        for (const auto& prm : params.kids) {
            s.names[prm->text] = s.count++;
            proto->param_hints.push_back(prm->kids.empty() ? "" : prm->kids[0]->text);
            if (!prm->kids.empty()) s.hints[prm->text] = prm->kids[0]->text;
        }
        proto->nslots = s.count;
        scopes_.push_back(std::move(s));
        const Ast& body = *n.kids[1];
        if (body.kind == Node::Block) {
            for (const auto& st : body.kids) stmt(*st);
            if (op_hint(n).empty()) {
                emit(Op::ReturnNone);
            } else {
                emit(Op::None);
                check_return(n);
                emit(Op::Return);
            }
        } else {
            expr(body);
            check_return(body);
            emit(Op::Return);
        }
        scopes_.pop_back();
        fns_.pop_back();
        return idx;
    }

    void expr(const Ast& n) {
        switch (n.kind) {
            case Node::Number:
                try {
                    emit(Op::Const, constant(op_number(n)));
                } catch (const RuntimeError& e) {
                    emit(Op::Fail, str(e.what()), 0, &n);
                }
                return;
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
                emit(Op::Binary, binop_code(n.text), 0, &n);
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
    explicit Machine(std::ostream& out) : host_(out) {}

    void run_main(const Ast& program, const std::string& path) {
        auto prog = std::make_unique<Program>();
        prog->file = path;
        Compiler(*prog).compile_main(program);
        Program* p = prog.get();
        if (!path.empty()) {
            add_value(*p, fs_stem(path));
            mods_[key_of(path)] = std::move(prog);
        } else {
            main_ = std::move(prog);
        }
        run(*p->protos[0], nullptr);
    }

private:
    Interpreter host_;  // owns the built-ins (print writes to its stream)
    std::unordered_map<std::string, std::unique_ptr<Program>> mods_;  // by resolved path
    std::unique_ptr<Program> main_;
    int depth_ = 0;

    static std::string fs_stem(const std::string& path) {
        size_t cut = path.find_last_of("/\\");
        std::string base = cut == std::string::npos ? path : path.substr(cut + 1);
        size_t dot = base.rfind('.');
        return dot == std::string::npos ? base : base.substr(0, dot);
    }

    static void add_value(Program& p, const std::string& name) {
        Program* raw = &p;
        auto m = std::make_shared<Module>();
        m->name = name;
        m->file = p.file;
        m->lookup = [raw](const std::string& n) { return raw->lookup(n); };
        p.value = m;
    }

    // Loads (once) the module a `use` names, relative to the file holding the `use`.
    Value load_module(const Instr& in, const Program& from, const std::string& use_path) {
        std::string file = module_path(from.file, use_path);
        std::string key = key_of(file);
        auto it = mods_.find(key);
        if (it != mods_.end()) return Value(it->second->value);  // loaded, or loading (a cycle)
        std::ifstream f(file, std::ios::binary);
        if (!f) fail_at(*in.at, "cannot find module '" + use_path + "' (no file " + use_path + ".relic)");
        std::stringstream ss;
        ss << f.rdbuf();
        auto prog = std::make_unique<Program>();
        prog->file = file;
        try {
            prog->tree = parse(lex(ss.str()));
        } catch (const LexError& e) {
            RuntimeError r(e.line, e.col, e.what());
            r.file = file;
            throw r;
        } catch (const ParseError& e) {
            RuntimeError r(e.line, e.col, e.what());
            r.file = file;
            throw r;
        }
        Compiler(*prog).compile_main(*prog->tree);
        add_value(*prog, use_path.substr(use_path.find_last_of('/') + 1));
        Program* p = prog.get();
        mods_[key] = std::move(prog);  // before running, so cycles find it
        try {
            run(*p->protos[0], nullptr);
        } catch (RuntimeError& e) {
            if (e.file.empty()) e.file = file;
            throw;
        }
        return Value(p->value);
    }

    const Value* builtin(const std::string& name) {
        auto it = host_.builtins()->vars.find(name);
        return it == host_.builtins()->vars.end() ? nullptr : &it->second;
    }

    Value call(const Instr& in, Value callee, std::vector<Value>& args) {
        if (callee.is<std::shared_ptr<Builtin>>())
            return op_call_builtin(*in.at, *callee.as<std::shared_ptr<Builtin>>(), args);
        if (!callee.is<std::shared_ptr<Function>>())
            fail_at(*in.at, std::string("cannot call ") + a_type(callee));
        const Function& f = *callee.as<std::shared_ptr<Function>>();
        if (!f.proto) fail_at(*in.at, "cannot call a function made outside the VM");
        const Proto& p = *static_cast<const Proto*>(f.proto);
        if (args.size() != static_cast<size_t>(p.nparams))
            fail_at(*in.at, op_arity_message(f.name, static_cast<size_t>(p.nparams), args.size()));
        gc_maybe();
        if (depth_ >= kMaxCallDepth)
            fail_at(*in.at, "too much recursion (more than " + std::to_string(kMaxCallDepth) + " calls deep)");
        for (size_t i = 0; i < args.size(); ++i)
            if (!p.param_hints[i].empty())
                op_check_type(*in.at, p.param_hints[i], args[i], f.name + "() argument '" + p.node->kids[0]->kids[i]->text + "' must be");
        auto scope = new_scope(p.nslots, std::static_pointer_cast<Scope>(f.vm_scope));
        for (size_t i = 0; i < args.size(); ++i) scope->slots[i] = std::move(args[i]);
        ++depth_;
        struct Guard { int& d; ~Guard() { --d; } } guard{depth_};
        if (p.mod == cur_mod_) return run(p, std::move(scope));
        try {
            return run(p, std::move(scope));
        } catch (RuntimeError& e) {
            if (e.file.empty()) e.file = p.mod->file;
            op_std_boundary(e, p.mod->file, cur_mod_ ? cur_mod_->file : std::string(), f.name, in.at->line, in.at->col);
            throw;
        }
    }

    const Program* cur_mod_ = nullptr;

    // Scopes nobody kept (no closure captured them) are reused, which saves
    // two allocations per loop turn with a `let` in it and per call.
    std::vector<std::shared_ptr<Scope>> spare_;
    std::vector<std::vector<Value>> spare_stacks_;
    std::shared_ptr<Scope> new_scope(int n, std::shared_ptr<Scope> parent) {
        if (!spare_.empty()) {
            std::shared_ptr<Scope> s = std::move(spare_.back());
            spare_.pop_back();
            s->slots.resize(static_cast<size_t>(n));
            s->parent = std::move(parent);
            return s;
        }
        return std::make_shared<Scope>(n, std::move(parent));
    }
    void recycle(std::shared_ptr<Scope>&& s) {
        if (s.use_count() != 1 || spare_.size() >= 64) return;
        for (Value& v : s->slots) v = Value();
        s->slots.clear();
        s->parent.reset();
        spare_.push_back(std::move(s));
    }

    Value run(const Proto& p, std::shared_ptr<Scope> sc) {
        Scope* const stop = sc ? sc->parent.get() : nullptr;
        auto leave = [&]() {  // hand this call's scopes back for reuse
            while (sc && sc.get() != stop) {
                std::shared_ptr<Scope> parent = sc->parent;
                recycle(std::move(sc));
                sc = std::move(parent);
            }
        };
        const Program* saved = cur_mod_;
        cur_mod_ = p.mod;
        struct Restore { const Program*& c; const Program* old; ~Restore() { c = old; } } restore{cur_mod_, saved};
        std::vector<Value>& globals_ = p.mod->globals;
        std::vector<char>& defined_ = p.mod->defined;
        // Value stacks are reused between calls too.
        std::vector<Value> st;
        if (!spare_stacks_.empty()) {
            st.swap(spare_stacks_.back());
            spare_stacks_.pop_back();
        } else {
            st.reserve(16);
        }
        struct GiveBack {
            std::vector<Value>& st;
            std::vector<std::vector<Value>>& pool;
            ~GiveBack() {
                if (pool.size() >= 64) return;
                st.clear();
                pool.push_back(std::move(st));
            }
        } give_back{st, spare_stacks_};
        auto pop = [&]() {
            Value v = std::move(st.back());
            st.pop_back();
            return v;
        };
        size_t ip = 0;
        const size_t n = p.code.size();
        struct Handler { size_t ip; size_t stack; std::shared_ptr<Scope> scope; };
        std::vector<Handler> handlers;
        for (;;) {
        try {
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
                    if (!b) {
                        std::vector<std::string> names;
                        if (in.op == Op::LoadBuiltin) {
                            const std::string& ls = p.strings[in.b];
                            for (size_t i = 0, j; (j = ls.find('\n', i)) != std::string::npos; i = j + 1)
                                names.push_back(ls.substr(i, j - i));
                        }
                        for (size_t g = 0; g < p.mod->global_names.size(); ++g)
                            if (defined_[g]) names.push_back(p.mod->global_names[g]);
                        for (const auto& kv : host_.builtins()->vars) names.push_back(kv.first);
                        fail_at(*in.at, "'" + name + "' is not defined" + op_suggest(name, names));
                    }
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
                case Op::EnterScope: sc = new_scope(in.a, std::move(sc)); break;
                case Op::ExitScope:
                    for (int i = 0; i < in.a; ++i) {
                        std::shared_ptr<Scope> parent = sc->parent;
                        recycle(std::move(sc));
                        sc = std::move(parent);
                    }
                    break;
                case Op::Jump:
                    if (static_cast<size_t>(in.a) < ip) gc_maybe();  // a loop turn
                    ip = static_cast<size_t>(in.a);
                    break;
                case Op::JumpIfFalse:
                    if (st.back().is<bool>()) {
                        if (!st.back().as<bool>()) ip = static_cast<size_t>(in.a);
                    } else if (!op_condition(*in.at, st.back(), p.strings[in.b].c_str())) ip = static_cast<size_t>(in.a);
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
                    // Fast paths for number math and compares, the hot case in loops.
                    // Anything unusual (overflow, division, mixed types) takes op_binary.
                    if (a.is<long long>() && b.is<long long>()) {
                        long long x = a.as<long long>(), y = b.as<long long>(), r;
                        bool done = true;
                        switch (in.a) {
                            case BAdd: if (add_overflows(x, y, &r)) done = false; else a = Value(r); break;
                            case BSub: if (sub_overflows(x, y, &r)) done = false; else a = Value(r); break;
                            case BMul: if (mul_overflows(x, y, &r)) done = false; else a = Value(r); break;
                            case BMod:
                                if (y <= 0) { done = false; break; }
                                r = x % y;
                                if (r < 0) r += y;
                                a = Value(r);
                                break;
                            case BEq: a = Value(x == y); break;
                            case BNe: a = Value(x != y); break;
                            case BLt: a = Value(x < y); break;
                            case BLe: a = Value(x <= y); break;
                            case BGt: a = Value(x > y); break;
                            case BGe: a = Value(x >= y); break;
                            default: done = false;
                        }
                        if (done) break;
                    } else if (a.is<double>() && b.is<double>()) {
                        double x = a.as<double>(), y = b.as<double>();
                        bool done = true;
                        switch (in.a) {
                            case BAdd: a = Value(x + y); break;
                            case BSub: a = Value(x - y); break;
                            case BMul: a = Value(x * y); break;
                            case BEq: a = Value(x == y); break;
                            case BNe: a = Value(x != y); break;
                            case BLt: a = Value(x < y); break;
                            case BLe: a = Value(x <= y); break;
                            case BGt: a = Value(x > y); break;
                            case BGe: a = Value(x >= y); break;
                            default: done = false;
                        }
                        if (done) break;
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
                    const Proto& fp = *p.mod->protos[in.a];
                    auto f = std::make_shared<Function>(fp.name, fp.node, nullptr, &fp, sc, std::string());
                    f->proto = &fp;
                    f->vm_scope = sc;
                    st.emplace_back(f);
                    break;
                }
                case Op::Return: {
                    Value r = pop();
                    leave();
                    return r;
                }
                case Op::ReturnNone: leave(); return Value();
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
                case Op::ForRangePrep: {
                    Value b = pop();
                    Value a = pop();
                    long long lo, hi;
                    op_range_bounds(*in.at, a, b, lo, hi);
                    st.emplace_back(hi);
                    st.emplace_back(lo);
                    break;
                }
                case Op::ForRangeNext: {
                    long long& i = std::get<long long>(st.back().v);
                    if (i < st[st.size() - 2].as<long long>()) {
                        st.emplace_back(i++);
                    } else {
                        st.pop_back();
                        st.pop_back();
                        ip = static_cast<size_t>(in.a);
                    }
                    break;
                }
                case Op::Fail: fail_at(*in.at, p.strings[in.a]);
                case Op::Use: st.push_back(load_module(in, *p.mod, p.strings[in.a])); break;
                case Op::TryBegin: handlers.push_back({static_cast<size_t>(in.a), st.size(), sc}); break;
                case Op::TryEnd: handlers.pop_back(); break;
                case Op::CheckType: op_check_type(*in.at, p.strings[in.a], st.back(), p.strings[in.b]); break;
            }
        }
        return Value();
        } catch (RuntimeError& e) {
            if (handlers.empty()) throw;
            Handler h = std::move(handlers.back());
            handlers.pop_back();
            st.resize(h.stack);
            sc = std::move(h.scope);
            st.push_back(op_error_value(e));
            ip = h.ip;
        }
        }
    }
};

}  // namespace

void vm_run(const Ast& program, std::ostream& out, const std::string& path) {
    Machine(out).run_main(program, path == "<repl>" ? "" : path);
}

int vm_check(const Ast& program, const std::string& path, std::string& out) {
    Program prog;
    Compiler(prog).compile_main(program);
    std::ostringstream sink;
    Interpreter host(sink);
    const auto& builtins = host.builtins()->vars;
    struct Item { int line, col; std::string msg; };
    std::vector<Item> items;
    for (const auto& p : prog.protos) {
        for (const Instr& in : p->code) {
            if (in.op == Op::Fail && in.at) {
                items.push_back({in.at->line, in.at->col, p->strings[in.a]});
            } else if (in.op == Op::LoadBuiltin && in.at) {
                const std::string& name = p->strings[in.a];
                if (builtins.count(name) || prog.global_index.count(name)) continue;
                std::vector<std::string> names = prog.global_names;
                const std::string& ls = p->strings[in.b];
                for (size_t i = 0, j; (j = ls.find('\n', i)) != std::string::npos; i = j + 1)
                    names.push_back(ls.substr(i, j - i));
                for (const auto& kv : builtins) names.push_back(kv.first);
                items.push_back({in.at->line, in.at->col, "'" + name + "' is not defined" + op_suggest(name, names)});
            }
        }
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.line != b.line ? a.line < b.line : a.col < b.col;
    });
    for (const auto& it : items) out += diag::format(path, it.line, it.col, it.msg, "warning");
    return static_cast<int>(items.size());
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
