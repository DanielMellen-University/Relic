#include "diag/diag.hpp"

#include <fstream>

namespace relic::diag {

// The source line an error points at, with a caret under the column:
//     print(cuont)
//           ^
// Empty when the file cannot be read or the line is not there.
std::string snippet(const std::string& path, int line, int col) {
    if (line < 1 || col < 1) return "";
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::string text;
    for (int i = 0; i < line; ++i)
        if (!std::getline(f, text)) return "";
    if (!text.empty() && text.back() == '\r') text.pop_back();
    if (text.find_first_not_of(" \t") == std::string::npos) return "";
    // Very long lines are cut to a window around the column. Columns count
    // characters, so the window start is found by walking that many
    // characters in, and the column is moved by the characters cut off.
    // The window itself stays 160 bytes, stepped to character edges.
    if (text.size() > 160) {
        size_t skip = static_cast<size_t>(col) > 80 ? static_cast<size_t>(col) - 80 : 0;
        size_t from = 0;
        size_t cut = 0;
        while (from < text.size() && cut < skip) {
            unsigned char c = static_cast<unsigned char>(text[from]);
            size_t len = (c & 0x80) == 0 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
            if (from + len > text.size()) len = text.size() - from;
            from += len;
            ++cut;
        }
        size_t to = from + 160;
        while (to < text.size() && (static_cast<unsigned char>(text[to]) & 0xC0) == 0x80) ++to;
        text = text.substr(from, to - from);
        col -= static_cast<int>(cut);
    }
    std::string pad;
    // Columns count characters: one pad space per character before the column.
    for (size_t i = 0, n = 0; n + 1 < static_cast<size_t>(col) && i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c & 0xC0) == 0x80) continue;
        pad += c == '\t' ? '\t' : ' ';
        ++n;
    }
    return "    " + text + "\n    " + pad + "^\n";
}

std::string format(const std::string& path, int line, int col, const std::string& msg, const char* kind) {
    return path + ":" + std::to_string(line) + ":" + std::to_string(col) + ": " + kind + ": " + msg + "\n" +
           snippet(path, line, col);
}

}  // namespace relic::diag
