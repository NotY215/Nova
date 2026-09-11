#include "Value.hpp"
#include <cmath>
#include <sstream>

namespace vayu {

    // ===========================================================================
    // Lifecycle
    // ===========================================================================

    void Value::destroy() noexcept {
        // Fast path: trivial alternatives (None/Bool/Int/Float) need no work.
        if (static_cast<uint8_t>(tag_) < static_cast<uint8_t>(Tag::Str)) return;
        switch (tag_) {
        case Tag::Str:      u_.s.~basic_string();      break;
        case Tag::Callable: u_.callable.~shared_ptr(); break;
        case Tag::Instance: u_.inst.~shared_ptr();     break;
        case Tag::Class:    u_.cls.~shared_ptr();      break;
        case Tag::List:     u_.list.~shared_ptr();     break;
        case Tag::Map:      u_.map.~shared_ptr();      break;
        case Tag::Module:   u_.module.~shared_ptr();   break;
        default: break;
        }
    }

    void Value::copyFrom(const Value& other) {
        tag_ = other.tag_;
        switch (tag_) {
        case Tag::None:   break;
        case Tag::Bool:   u_.b = other.u_.b; break;
        case Tag::Int:    u_.i = other.u_.i; break;
        case Tag::Float:  u_.f = other.u_.f; break;
        case Tag::Str:      new (&u_.s) std::string(other.u_.s);         break;
        case Tag::Callable: new (&u_.callable) FnPtr(other.u_.callable); break;
        case Tag::Instance: new (&u_.inst) InstPtr(other.u_.inst);       break;
        case Tag::Class:    new (&u_.cls) ClassPtr(other.u_.cls);        break;
        case Tag::List:     new (&u_.list) ListPtr(other.u_.list);       break;
        case Tag::Map:      new (&u_.map) MapPtr(other.u_.map);          break;
        case Tag::Module:   new (&u_.module) ModulePtr(other.u_.module); break;
        }
    }

    void Value::moveFrom(Value&& other) noexcept {
        tag_ = other.tag_;
        switch (tag_) {
        case Tag::None:   break;
        case Tag::Bool:   u_.b = other.u_.b; break;
        case Tag::Int:    u_.i = other.u_.i; break;
        case Tag::Float:  u_.f = other.u_.f; break;
        case Tag::Str:      new (&u_.s) std::string(std::move(other.u_.s));         break;
        case Tag::Callable: new (&u_.callable) FnPtr(std::move(other.u_.callable)); break;
        case Tag::Instance: new (&u_.inst) InstPtr(std::move(other.u_.inst));       break;
        case Tag::Class:    new (&u_.cls) ClassPtr(std::move(other.u_.cls));        break;
        case Tag::List:     new (&u_.list) ListPtr(std::move(other.u_.list));       break;
        case Tag::Map:      new (&u_.map) MapPtr(std::move(other.u_.map));          break;
        case Tag::Module:   new (&u_.module) ModulePtr(std::move(other.u_.module)); break;
        }
        // Deliberately do NOT reset other.tag_.  Moved-from strings and
        // shared_ptrs remain valid objects whose destructors must run.
    }

    Value::Value(std::string s) : tag_(Tag::Str) {
        new (&u_.s) std::string(std::move(s));
    }

    Value::Value(const char* s) : tag_(Tag::Str) {
        new (&u_.s) std::string(s);
    }

    Value::Value(FnPtr c) noexcept : tag_(Tag::Callable) { new (&u_.callable) FnPtr(std::move(c)); }
    Value::Value(InstPtr s) noexcept : tag_(Tag::Instance) { new (&u_.inst) InstPtr(std::move(s)); }
    Value::Value(ClassPtr c) noexcept : tag_(Tag::Class) { new (&u_.cls) ClassPtr(std::move(c)); }
    Value::Value(ListPtr l) noexcept : tag_(Tag::List) { new (&u_.list) ListPtr(std::move(l)); }
    Value::Value(MapPtr m) noexcept : tag_(Tag::Map) { new (&u_.map) MapPtr(std::move(m)); }
    Value::Value(ModulePtr m) noexcept : tag_(Tag::Module) { new (&u_.module) ModulePtr(std::move(m)); }

    Value::Value(const Value& other) { copyFrom(other); }
    Value::Value(Value&& other) noexcept { moveFrom(std::move(other)); }
    Value::~Value() { destroy(); }

    Value& Value::operator=(const Value& other) {
        if (this != &other) { destroy(); copyFrom(other); }
        return *this;
    }
    Value& Value::operator=(Value&& other) noexcept {
        if (this != &other) { destroy(); moveFrom(std::move(other)); }
        return *this;
    }

    // ===========================================================================
    // Conversions
    // ===========================================================================

    static std::string formatDouble(double d) {
        if (std::isnan(d)) return "nan";
        if (std::isinf(d)) return d < 0 ? "-inf" : "inf";
        std::ostringstream oss; oss.precision(15); oss << d;
        std::string s = oss.str();
        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) s += ".0";
        return s;
    }

    bool Value::truthy() const {
        switch (tag_) {
        case Tag::None:  return false;
        case Tag::Bool:  return u_.b;
        case Tag::Int:   return u_.i != 0;
        case Tag::Float: return u_.f != 0.0;
        case Tag::Str:   return !u_.s.empty();
        case Tag::List:  return !u_.list->items.empty();
        case Tag::Map:   return !u_.map->entries.empty();
        default:         return true;   // callables, instances, classes, modules
        }
    }

    std::string Value::toString() const {
        switch (tag_) {
        case Tag::None:   return "None";
        case Tag::Bool:   return u_.b ? "true" : "false";
        case Tag::Int:    return std::to_string(u_.i);
        case Tag::Float:  return formatDouble(u_.f);
        case Tag::Str:    return u_.s;
        case Tag::Callable: return "<function " + u_.callable->name + ">";
        case Tag::Class:  return "<class " + u_.cls->name + ">";
        case Tag::Module: return "<module " + u_.module->name + ">";

        case Tag::List: {
            std::string out = "[";
            const auto& items = u_.list->items;
            for (size_t i = 0; i < items.size(); ++i) {
                if (i) out += ", ";
                if (items[i].isString()) out += "\"" + items[i].asString() + "\"";
                else                     out += items[i].toString();
            }
            out += "]";
            return out;
        }
        case Tag::Map: {
            std::string out = "{";
            bool first = true;
            for (auto& [k, v] : u_.map->entries) {
                if (!first) out += ", ";
                first = false;
                out += "\"" + k + "\": ";
                if (v.isString()) out += "\"" + v.asString() + "\"";
                else              out += v.toString();
            }
            out += "}";
            return out;
        }
        case Tag::Instance: {
            auto s = u_.inst;
            std::string out = s->cls ? s->cls->name : "?";
            out += "(";
            bool first = true;
            auto emit = [&](const std::string& fn, const Value& v) {
                if (!first) out += ", ";
                first = false;
                out += fn + "=";
                if (v.isString()) out += "\"" + v.asString() + "\"";
                else              out += v.toString();
                };
            if (s->cls) {
                for (const auto& fn : s->cls->fieldOrder) {
                    auto it = s->fields.find(fn);
                    if (it != s->fields.end()) emit(fn, it->second);
                }
            }
            for (auto& [fn, v] : s->fields) {
                if (s->cls) {
                    bool declared = false;
                    for (auto& d : s->cls->fieldOrder) if (d == fn) { declared = true; break; }
                    if (declared) continue;
                }
                emit(fn, v);
            }
            out += ")";
            return out;
        }
        }
        return "<unknown>";
    }

    std::string Value::typeName() const {
        switch (tag_) {
        case Tag::None:     return "None";
        case Tag::Bool:     return "bool";
        case Tag::Int:      return "int";
        case Tag::Float:    return "float";
        case Tag::Str:      return "str";
        case Tag::Callable: return "function";
        case Tag::Instance: return u_.inst->cls ? u_.inst->cls->name : "?";
        case Tag::Class:    return u_.cls->name;
        case Tag::List:     return "list";
        case Tag::Map:      return "map";
        case Tag::Module:   return "module";
        }
        return "?";
    }

} // namespace vayu