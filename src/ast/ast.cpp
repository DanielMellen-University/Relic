#include "ast/ast.hpp"

namespace relic {

const char* node_name(Node k) {
    switch (k) {
        case Node::Program: return "Program";
        case Node::Block: return "Block";
        case Node::Let: return "Let";
        case Node::Assign: return "Assign";
        case Node::ExprStmt: return "ExprStmt";
        case Node::If: return "If";
        case Node::While: return "While";
        case Node::For: return "For";
        case Node::Fun: return "Fun";
        case Node::Params: return "Params";
        case Node::Return: return "Return";
        case Node::Break: return "Break";
        case Node::Next: return "Next";
        case Node::Use: return "Use";
        case Node::Number: return "Number";
        case Node::String: return "String";
        case Node::Interp: return "Interp";
        case Node::Name: return "Name";
        case Node::True: return "True";
        case Node::False: return "False";
        case Node::None: return "None";
        case Node::List: return "List";
        case Node::Map: return "Map";
        case Node::Pair: return "Pair";
        case Node::Unary: return "Unary";
        case Node::Binary: return "Binary";
        case Node::Range: return "Range";
        case Node::Call: return "Call";
        case Node::Index: return "Index";
        case Node::Field: return "Field";
    }
    return "?";
}

namespace {

std::string quote(const std::string& s) {
    std::string q = "\"";
    for (char c : s) {
        if (c == '\n') q += "\\n";
        else if (c == '\t') q += "\\t";
        else if (c == '"') q += "\\\"";
        else if (c == '\\') q += "\\\\";
        else q += c;
    }
    return q + "\"";
}

void dump(const Ast& n, int depth, std::string& out) {
    out.append(static_cast<size_t>(depth) * 2, ' ');
    out += node_name(n.kind);
    if (n.kind == Node::String) out += " " + quote(n.text);
    else if (!n.text.empty()) out += " " + n.text;
    out += "\n";
    for (const auto& k : n.kids) dump(*k, depth + 1, out);
}

}  // namespace

std::string dump_ast(const Ast& node) {
    std::string out;
    dump(node, 0, out);
    return out;
}

}  // namespace relic
