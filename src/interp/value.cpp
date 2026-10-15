#include "interp/value.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace relic {

bool Map::has(const std::string& k) const { return items.count(k) != 0; }

void Map::set(const std::string& k, Value v) {
    if (!has(k)) keys.push_back(k);
    items[k] = std::move(v);
}

// Variant order: none, bool, float, int, string, list, map, function, builtin, module.
const char* type_name(const Value& v) {
    switch (v.v.index()) {
        case 0: return "none";
        case 1: return "bool";
        case 2: return "float";
        case 3: return "int";
        case 4: return "string";
        case 5: return "list";
        case 6: return "map";
        case 9: return "module";
        default: return "function";
    }
}

std::string a_type(const Value& v) {
    if (v.is<long long>()) return "an int";
    return std::string("a ") + type_name(v);
}

namespace {

// Floats always show a decimal point or an exponent, so 3.0 and 3 look different.
std::string float_text(double d) {
    if (std::isnan(d)) return "nan";
    if (std::isinf(d)) return d > 0 ? "inf" : "-inf";
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.15g", d);
    std::string s = buf;
    if (s.find_first_of(".en") == std::string::npos) s += ".0";
    return s;
}

bool is_plain_name(const std::string& s) {
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
    for (char c : s)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    return true;
}

std::string quoted(const std::string& s) {
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

}  // namespace

std::string show(const Value& v) {
    if (v.is<std::string>()) return v.as<std::string>();
    return repr(v);
}

std::string repr(const Value& v) {
    switch (v.v.index()) {
        case 0: return "none";
        case 1: return v.as<bool>() ? "true" : "false";
        case 2: return float_text(v.as<double>());
        case 3: return std::to_string(v.as<long long>());
        case 4: return quoted(v.as<std::string>());
        case 5: {
            std::string s = "[";
            const List& l = *v.as<std::shared_ptr<List>>();
            for (size_t i = 0; i < l.size(); ++i) s += (i ? ", " : "") + repr(l[i]);
            return s + "]";
        }
        case 6: {
            const Map& m = *v.as<std::shared_ptr<Map>>();
            std::string s = "{";
            for (size_t i = 0; i < m.keys.size(); ++i) {
                const std::string& k = m.keys[i];
                s += (i ? ", " : "") + (is_plain_name(k) ? k : quoted(k)) + ": " +
                     repr(m.items.at(k));
            }
            return s + "}";
        }
        case 7: return "<fun " + v.as<std::shared_ptr<Function>>()->name + ">";
        case 8: return "<fun " + v.as<std::shared_ptr<Builtin>>()->name + ">";
        default: return "<module " + v.as<std::shared_ptr<Module>>()->name + ">";
    }
}

size_t utf8_char_size(const std::string& s, size_t pos) {
    unsigned char c = static_cast<unsigned char>(s[pos]);
    size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
    if (n == 1 || pos + n > s.size()) return 1;
    for (size_t i = 1; i < n; ++i)
        if ((static_cast<unsigned char>(s[pos + i]) & 0xC0) != 0x80) return 1;
    return n;
}

// Length of the leading run of ASCII bytes, checked 8 bytes at a time.
static size_t ascii_prefix(const std::string& s, size_t limit) {
    size_t i = 0;
    const char* p = s.data();
    while (i + 8 <= limit) {
        unsigned long long w;
        std::memcpy(&w, p + i, 8);
        if (w & 0x8080808080808080ULL) break;
        i += 8;
    }
    while (i < limit && static_cast<unsigned char>(p[i]) < 0x80) ++i;
    return i;
}

size_t utf8_len(const std::string& s) {
    size_t i = ascii_prefix(s, s.size()), n = i;
    while (i < s.size()) {
        i += utf8_char_size(s, i);
        ++n;
    }
    return n;
}

size_t utf8_offset(const std::string& s, size_t index) {
    size_t limit = index < s.size() ? index + 1 : s.size();
    size_t i = ascii_prefix(s, limit);
    if (i == limit) return index < s.size() ? index : s.size();
    size_t n = i;
    while (i < s.size() && n < index) {
        i += utf8_char_size(s, i);
        ++n;
    }
    return i;
}

long utf8_decode(const std::string& s, size_t pos) {
    size_t n = utf8_char_size(s, pos);
    unsigned char c = static_cast<unsigned char>(s[pos]);
    if (n == 1) return c;
    long cp = c & (0x7F >> n);
    for (size_t i = 1; i < n; ++i) cp = (cp << 6) | (static_cast<unsigned char>(s[pos + i]) & 0x3F);
    return cp;
}

std::string utf8_encode(long cp) {
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

bool equal(const Value& a, const Value& b) {
    if (is_num(a) && is_num(b)) {
        if (a.is<long long>() && b.is<long long>()) return a.as<long long>() == b.as<long long>();
        return to_num(a) == to_num(b);
    }
    if (a.v.index() != b.v.index()) return false;
    switch (a.v.index()) {
        case 0: return true;
        case 1: return a.as<bool>() == b.as<bool>();
        case 4: return a.as<std::string>() == b.as<std::string>();
        case 5: {
            const List& x = *a.as<std::shared_ptr<List>>();
            const List& y = *b.as<std::shared_ptr<List>>();
            if (x.size() != y.size()) return false;
            for (size_t i = 0; i < x.size(); ++i)
                if (!equal(x[i], y[i])) return false;
            return true;
        }
        case 6: {
            const Map& x = *a.as<std::shared_ptr<Map>>();
            const Map& y = *b.as<std::shared_ptr<Map>>();
            if (x.keys.size() != y.keys.size()) return false;
            for (const auto& k : x.keys)
                if (!y.has(k) || !equal(x.items.at(k), y.items.at(k))) return false;
            return true;
        }
        case 7: return a.as<std::shared_ptr<Function>>() == b.as<std::shared_ptr<Function>>();
        case 8: return a.as<std::shared_ptr<Builtin>>() == b.as<std::shared_ptr<Builtin>>();
        default: return a.as<std::shared_ptr<Module>>() == b.as<std::shared_ptr<Module>>();
    }
}

}  // namespace relic
