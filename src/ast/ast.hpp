#pragma once
#include <memory>
#include <string>
#include <vector>

namespace relic {

// One node type for the whole tree. `kind` says what it is, `text` holds a
// name, operator or literal, and `kids` holds child nodes in a fixed order
// per kind (listed below). Kept simple so the tree-walker and --ast can
// share it.
enum class Node {
    // statements
    Program,   // kids: statements
    Block,     // kids: statements
    Let,       // text: name. kids: value, [Type]
    Assign,    // kids: target (Name, Index or Field), value
    ExprStmt,  // kids: expression
    If,        // kids: cond, then Block, [else: Block or If]
    While,     // kids: cond, Block
    For,       // text: loop name. kids: iterable, Block
    Fun,       // text: name. kids: Params, body (Block, or one expression for the short form), [Type]
    Params,    // kids: Name nodes. Each Name's kids: [Type], then [default expression]
    Return,    // kids: [value]
    Break,
    Next,
    Use,       // text: module path as written, e.g. "lib/math" or "../util". kids: [Name for `as`]
    Try,       // text: catch name (may be empty). kids: try Block, catch Block
    Type,      // text: a type hint after `as`: int, float, string, ...
    // expressions
    Number,    // text: digits as written
    String,    // text: decoded value
    Interp,    // kids: String and expression parts, in order
    Name,      // text: name
    True,
    False,
    None,
    List,      // kids: items
    Map,       // kids: Pair nodes
    Pair,      // text: key. kids: value
    Unary,     // text: "-" or "not". kids: operand
    Binary,    // text: operator. kids: left, right
    Range,     // kids: start, end
    Call,      // kids: callee, args...
    Index,     // kids: object, index
    Field,     // text: field name. kids: object
};

struct Ast {
    Node kind;
    std::string text;
    int line = 0;
    int col = 0;
    std::vector<std::unique_ptr<Ast>> kids;
};

using AstPtr = std::unique_ptr<Ast>;

const char* node_name(Node kind);

// Prints the tree, one node per line, two spaces per level.
std::string dump_ast(const Ast& node);

// A parameter's optional `as` hint and its optional default expression.
// The hint, when present, is the first kid. The default, when present, is
// the last kid and is never a Type node.
inline const Ast* param_type(const Ast& prm) {
    if (!prm.kids.empty() && prm.kids[0]->kind == Node::Type) return prm.kids[0].get();
    return nullptr;
}
inline const Ast* param_default(const Ast& prm) {
    if (prm.kids.empty() || prm.kids.back()->kind == Node::Type) return nullptr;
    return prm.kids.back().get();
}
inline std::string param_hint_text(const Ast& prm) {
    const Ast* t = param_type(prm);
    return t ? t->text : std::string();
}

}  // namespace relic
