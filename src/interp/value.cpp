#include "interp/value.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace relic {

void trace_value(const Value& v, std::vector<GcObj*>& out) {
    if (v.is<std::shared_ptr<List>>()) out.push_back(v.as<std::shared_ptr<List>>().get());
    else if (v.is<std::shared_ptr<Map>>()) out.push_back(v.as<std::shared_ptr<Map>>().get());
    else if (v.is<std::shared_ptr<Function>>()) out.push_back(v.as<std::shared_ptr<Function>>().get());
}

// A list inside a list inside a list... would free itself one C++ call per
// level and overflow the stack. Instead each dying container hands children
// it alone owns to this queue, and the outermost destructor empties it.
namespace {
std::vector<Value>* g_dying = nullptr;

bool sole_container(const Value& v) {
    if (v.is<std::shared_ptr<List>>()) return v.as<std::shared_ptr<List>>().use_count() == 1;
    if (v.is<std::shared_ptr<Map>>()) return v.as<std::shared_ptr<Map>>().use_count() == 1;
    return false;
}

template <class Each>
void free_flat(Each each) {
    bool outer = g_dying == nullptr;
    std::vector<Value> queue;
    if (outer) g_dying = &queue;
    each([](Value& v) {
        if (sole_container(v)) g_dying->push_back(std::move(v));
    });
    if (!outer) return;
    while (!queue.empty()) {
        Value v;
        v.v.swap(queue.back().v);  // (a swap: GCC 14 warns, wrongly, about a move here)
        queue.pop_back();
    }  // each v's destructor may push more onto the queue
    g_dying = nullptr;
}
}  // namespace

List::List(const List& o) : GcObj(o), items_(o.begin(), o.end()) {}

List& List::operator=(const List& o) {
    if (this != &o) {
        std::vector<Value> copy(o.begin(), o.end());
        items_.swap(copy);
        head_ = 0;
    }
    return *this;
}

void List::clear() {
    items_.clear();
    head_ = 0;
}

List::iterator List::insert(const_iterator pos, Value v) {
    size_t i = static_cast<size_t>(pos - begin());
    if (head_ > 0 && i < size() / 2) {
        // the items before i move down into the free slot in front
        --head_;
        std::move(begin() + 1, begin() + 1 + i, begin());
        (*this)[i] = std::move(v);
    } else {
        items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(head_ + i), std::move(v));
    }
    return begin() + i;
}

List::iterator List::erase(const_iterator pos) {
    size_t i = static_cast<size_t>(pos - begin());
    if (i < size() / 2) {
        // the items before i move up one, and the list starts a slot later
        std::move_backward(begin(), begin() + i, begin() + i + 1);
        items_[head_] = Value();
        ++head_;
        if (head_ > 16 && head_ * 2 > items_.size()) {
            items_.erase(items_.begin(), items_.begin() + static_cast<std::ptrdiff_t>(head_));
            head_ = 0;
        }
    } else {
        items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(head_ + i));
    }
    return begin() + i;
}

List::~List() {
    free_flat([this](auto keep) {
        for (Value& v : *this) keep(v);
    });
}

Map::~Map() {
    free_flat([this](auto keep) {
        for (auto& kv : items) keep(kv.second);
    });
}

void List::trace(std::vector<GcObj*>& out) const {
    for (const Value& v : *this) trace_value(v, out);
}
void List::clear_refs() {
    std::vector<Value> gone;
    gone.swap(items_);
    head_ = 0;
}

void Map::trace(std::vector<GcObj*>& out) const {
    for (const auto& kv : items) trace_value(kv.second, out);
}
void Map::clear_refs() {
    std::unordered_map<std::string, Value> gone;
    gone.swap(items);
    order_.clear();
    gone_.clear();
    pos_.clear();
    holes_ = 0;
}

void Function::clear_refs() {
    auto c = std::move(closure);
    auto s = std::move(vm_scope);
}

bool Map::has(const std::string& k) const { return items.count(k) != 0; }

void Map::set(const std::string& k, Value v) {
    auto it = items.find(k);
    if (it != items.end()) {
        it->second = std::move(v);
        return;
    }
    if (!gone_.empty()) {
        pos_[k] = order_.size();
        gone_.push_back(0);
    }
    order_.push_back(k);
    items.emplace(k, std::move(v));
}

bool Map::remove(const std::string& k) {
    if (!items.erase(k)) return false;
    if (gone_.empty()) {
        // first remove: index the order (once per map)
        gone_.assign(order_.size(), 0);
        pos_.reserve(order_.size());
        for (size_t i = 0; i < order_.size(); ++i) pos_[order_[i]] = i;
    }
    auto p = pos_.find(k);
    gone_[p->second] = 1;
    order_[p->second].clear();  // the hole keeps no copy of the key
    pos_.erase(p);
    ++holes_;
    if (holes_ > 16 && holes_ * 2 > order_.size()) compact();
    return true;
}

void Map::compact() {
    size_t j = 0;
    for (size_t i = 0; i < order_.size(); ++i) {
        if (gone_[i]) continue;
        if (j != i) order_[j] = std::move(order_[i]);
        pos_[order_[j]] = j;
        ++j;
    }
    order_.resize(j);
    gone_.assign(j, 0);
    holes_ = 0;
}

std::vector<std::string> Map::key_list() const {
    std::vector<std::string> out;
    out.reserve(size());
    each_key([&](const std::string& k) { out.push_back(k); });
    return out;
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

// A list or map that holds itself prints as [...] or {...} where it repeats,
// and so does anything nested deeper than kMaxShowDepth.
static constexpr size_t kMaxShowDepth = 1000;

static std::string repr_in(const Value& v, std::vector<const void*>& path);

std::string repr(const Value& v) {
    std::vector<const void*> path;
    return repr_in(v, path);
}

static std::string repr_in(const Value& v, std::vector<const void*>& path) {
    if (v.is<std::shared_ptr<List>>() || v.is<std::shared_ptr<Map>>()) {
        bool list = v.is<std::shared_ptr<List>>();
        const void* id = list ? static_cast<const void*>(v.as<std::shared_ptr<List>>().get())
                              : static_cast<const void*>(v.as<std::shared_ptr<Map>>().get());
        bool seen = false;
        for (const void* p : path) seen = seen || p == id;
        if (seen || path.size() >= kMaxShowDepth) return list ? "[...]" : "{...}";
        path.push_back(id);
        struct Pop { std::vector<const void*>& p; ~Pop() { p.pop_back(); } } pop{path};
        std::string s = list ? "[" : "{";
        if (list) {
            const List& l = *v.as<std::shared_ptr<List>>();
            for (size_t i = 0; i < l.size(); ++i) s += (i ? ", " : "") + repr_in(l[i], path);
            return s + "]";
        }
        const Map& m = *v.as<std::shared_ptr<Map>>();
        bool first = true;
        m.each_key([&](const std::string& k) {
            s += (first ? "" : ", ") + (is_plain_name(k) ? k : quoted(k)) + ": " + repr_in(m.items.at(k), path);
            first = false;
        });
        return s + "}";
    }
    switch (v.v.index()) {
        case 0: return "none";
        case 1: return v.as<bool>() ? "true" : "false";
        case 2: return float_text(v.as<double>());
        case 3: return std::to_string(v.as<long long>());
        case 4: return quoted(v.as<std::string>());
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

static size_t utf8_count_from(const std::string& s, size_t i, size_t n) {
    while (i < s.size()) {
        i += utf8_char_size(s, i);
        ++n;
    }
    return n;
}

// Long strings with multi-byte characters keep, in their buffer, the byte
// offset of every 64th character, like the C runtime's table, so an index
// walks at most 63 characters (built at the first index past kMarkMin bytes).
namespace {
constexpr size_t kMarkEvery = 64;
constexpr size_t kMarkMin = 256;  // shorter strings just walk

void count_chars(const StrBuf& b) {
    const std::string& s = b.s;
    size_t i = ascii_prefix(s, s.size());
    if (i == s.size()) {
        b.chars = i;
        return;
    }
    if (s.size() < kMarkMin) {
        b.chars = utf8_count_from(s, i, i);
        return;
    }
    b.marks.clear();
    size_t n = 0;
    i = 0;
    while (i < s.size()) {
        if (n % kMarkEvery == 0) b.marks.push_back(i);
        i += utf8_char_size(s, i);
        ++n;
    }
    b.chars = n;
}
}  // namespace

size_t utf8_len(const std::string& s) {
    size_t i = ascii_prefix(s, s.size());
    return i == s.size() ? i : utf8_count_from(s, i, i);
}

size_t str_len(const Value& v) {
    const StrBuf& b = *std::get<Str>(v.v).p;
    if (b.chars == static_cast<size_t>(-1)) count_chars(b);
    return b.chars;
}

size_t str_offset(const Value& v, size_t index) {
    const StrBuf& b = *std::get<Str>(v.v).p;
    const std::string& s = b.s;
    if (b.chars == static_cast<size_t>(-1)) {
        // short or not looked at yet: an ASCII start needs no count
        size_t limit = index < s.size() ? index + 1 : s.size();
        if (ascii_prefix(s, limit) == limit) return index < s.size() ? index : s.size();
        count_chars(b);
    }
    if (b.chars == s.size()) return index < s.size() ? index : s.size();  // all ASCII
    if (index >= b.chars) return s.size();
    size_t i = 0, n = 0;
    if (!b.marks.empty()) {
        i = b.marks[index / kMarkEvery];
        n = index - index % kMarkEvery;
    }
    while (i < s.size() && n < index) {
        i += utf8_char_size(s, i);
        ++n;
    }
    return i;
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

// Deeper than this, two structures count as different rather than
// overflowing the C++ stack.
static constexpr int kMaxEqualDepth = 10000;
static bool equal_at(const Value& a, const Value& b, int depth);

bool equal(const Value& a, const Value& b) { return equal_at(a, b, 0); }

static bool equal_at(const Value& a, const Value& b, int depth) {
    if (depth > kMaxEqualDepth) return false;
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
            if (&x == &y) return true;  // same list (also stops a list that holds itself)
            if (x.size() != y.size()) return false;
            for (size_t i = 0; i < x.size(); ++i)
                if (!equal_at(x[i], y[i], depth + 1)) return false;
            return true;
        }
        case 6: {
            const Map& x = *a.as<std::shared_ptr<Map>>();
            const Map& y = *b.as<std::shared_ptr<Map>>();
            if (&x == &y) return true;
            if (x.size() != y.size()) return false;
            for (const auto& kv : x.items) {
                auto it = y.items.find(kv.first);
                if (it == y.items.end() || !equal_at(kv.second, it->second, depth + 1)) return false;
            }
            return true;
        }
        case 7: return a.as<std::shared_ptr<Function>>() == b.as<std::shared_ptr<Function>>();
        case 8: return a.as<std::shared_ptr<Builtin>>() == b.as<std::shared_ptr<Builtin>>();
        default: return a.as<std::shared_ptr<Module>>() == b.as<std::shared_ptr<Module>>();
    }
}

}  // namespace relic
