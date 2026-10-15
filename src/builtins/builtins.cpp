// Built-in functions for Relic 1.0: print, len, append, str, keys, type.
#include "interp/interp.hpp"

namespace relic {

void Interpreter::install_builtins() {
    auto add = [&](const std::string& name, int arity, std::function<Value(std::vector<Value>&)> fn) {
        builtins_->vars[name] = Value(std::make_shared<Builtin>(Builtin{name, arity, std::move(fn)}));
    };

    // print(a, b, ...) writes the values separated by spaces, then a newline.
    add("print", -1, [this](std::vector<Value>& a) {
        for (size_t i = 0; i < a.size(); ++i) out_ << (i ? " " : "") << show(a[i]);
        out_ << "\n";
        return Value();
    });

    add("len", 1, [](std::vector<Value>& a) {
        const Value& v = a[0];
        if (v.is<std::string>()) return Value(static_cast<double>(v.as<std::string>().size()));
        if (v.is<std::shared_ptr<List>>())
            return Value(static_cast<double>(v.as<std::shared_ptr<List>>()->size()));
        if (v.is<std::shared_ptr<Map>>())
            return Value(static_cast<double>(v.as<std::shared_ptr<Map>>()->keys.size()));
        throw std::string("cannot get the length of a ") + type_name(v);
    });

    add("append", 2, [](std::vector<Value>& a) {
        if (!a[0].is<std::shared_ptr<List>>())
            throw std::string("first argument must be a list, got ") + type_name(a[0]);
        a[0].as<std::shared_ptr<List>>()->push_back(a[1]);
        return Value();
    });

    add("str", 1, [](std::vector<Value>& a) { return Value(show(a[0])); });

    add("keys", 1, [](std::vector<Value>& a) {
        if (!a[0].is<std::shared_ptr<Map>>())
            throw std::string("argument must be a map, got ") + type_name(a[0]);
        auto l = std::make_shared<List>();
        for (const auto& k : a[0].as<std::shared_ptr<Map>>()->keys) l->emplace_back(k);
        return Value(l);
    });

    add("type", 1, [](std::vector<Value>& a) { return Value(std::string(type_name(a[0]))); });
}

}  // namespace relic
