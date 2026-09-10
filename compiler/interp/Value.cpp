#include "Value.hpp"
#include <cmath>
#include <sstream>

namespace nova {

    static std::string formatDouble(double d) {
        if (std::isnan(d)) return "nan";
        if (std::isinf(d)) return d < 0 ? "-inf" : "inf";
        std::ostringstream oss; oss.precision(15); oss << d;
        std::string s = oss.str();
        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) s += ".0";
        return s;
    }

    bool Value::truthy() const {
        if (isNone())   return false;
        if (isBool())   return asBool();
        if (isInt())    return asInt() != 0;
        if (isFloat())  return asFloat() != 0.0;
        if (isString()) return !asString().empty();
        if (isList())   return !asList()->items.empty();
        if (isMap())    return !asMap()->entries.empty();
        return true;
    }

    std::string Value::toString() const {
        if (isNone())   return "None";
        if (isBool())   return asBool() ? "true" : "false";
        if (isInt())    return std::to_string(asInt());
        if (isFloat())  return formatDouble(asFloat());
        if (isString()) return asString();
        if (isCallable()) return "<function " + asCallable()->name + ">";
        if (isClass())  return "<class " + asClass()->name + ">";

        if (isList()) {
            std::string out = "[";
            const auto& items = asList()->items;
            for (size_t i = 0; i < items.size(); ++i) {
                if (i) out += ", ";
                if (items[i].isString()) out += "\"" + items[i].asString() + "\"";
                else                     out += items[i].toString();
            }
            out += "]";
            return out;
        }
        if (isMap()) {
            std::string out = "{";
            bool first = true;
            for (auto& [k, v] : asMap()->entries) {
                if (!first) out += ", ";
                first = false;
                out += "\"" + k + "\": ";
                if (v.isString()) out += "\"" + v.asString() + "\"";
                else              out += v.toString();
            }
            out += "}";
            return out;
        }

        if (isInstance()) {
            auto s = asInstance();
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
        return "<unknown>";
    }

    std::string Value::typeName() const {
        if (isNone())     return "None";
        if (isBool())     return "bool";
        if (isInt())      return "int";
        if (isFloat())    return "float";
        if (isString())   return "str";
        if (isCallable()) return "function";
        if (isInstance()) return asInstance()->cls ? asInstance()->cls->name : "?";
        if (isClass())    return asClass()->name;
        if (isList())     return "list";
        if (isMap())      return "map";
        return "?";
    }

} // namespace nova