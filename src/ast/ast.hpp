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
    Let,       // text: name. kids: value
    Assign,    // kids: target (Name, Index or Field), value
    ExprStmt,  // kids: expression
    If,        // kids: cond, then Block, [else: Block or If]
    While,     // kids: cond, Block
    For,       // text: loop name. kids: iterable, Block
    Fun,       // text: name. kids: Params, body (Block, or one expression for the short form)
    Params,    // kids: Name nodes
    Return,    // kids: [value]
    Break,
    Next,
    Use,       // text: module path as written, e.g. "lib/math" or "../util"
    Try,       // text: catch name (may be empty). kids: try Block, catch Block
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

}  // namespace relic
