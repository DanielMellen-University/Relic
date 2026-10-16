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
    std::string pad;
    // Columns count bytes; the caret lines up by characters.
    for (size_t i = 0; i + 1 < static_cast<size_t>(col) && i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c & 0xC0) == 0x80) continue;
        pad += c == '\t' ? '\t' : ' ';
    }
    return "    " + text + "\n    " + pad + "^\n";
}

std::string format(const std::string& path, int line, int col, const std::string& msg, const char* kind) {
    return path + ":" + std::to_string(line) + ":" + std::to_string(col) + ": " + kind + ": " + msg + "\n" +
           snippet(path, line, col);
}

}  // namespace relic::diag
