#include "Value.hpp"
#include <cmath>
#include <sstream>

namespace nova {

    static std::string formatDouble(double d) {
        if (std::isnan(d)) return "nan";
        if (std::isinf(d)) return d < 0 ? "-inf" : "inf";
        std::ostringstream oss;
        oss.precision(15);
        oss << d;
        std::string s = oss.str();
        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
            s += ".0";
        return s;
    }

    bool Value::truthy() const {
        if (isNone())   return false;
        if (isBool())   return asBool();
        if (isInt())    return asInt() != 0;
        if (isFloat())  return asFloat() != 0.0;
        if (isString()) return !asString().empty();
        return true;
    }

    std::string Value::toString() const {
        if (isNone())   return "None";
        if (isBool())   return asBool() ? "true" : "false";
        if (isInt())    return std::to_string(asInt());
        if (isFloat())  return formatDouble(asFloat());
        if (isString()) return asString();
        if (isCallable()) return "<function " + asCallable()->name + ">";
        if (isStruct()) {
            auto s = asStruct();
            std::string out = s->typeName + "(";
            for (size_t i = 0; i < s->fieldOrder.size(); ++i) {
                if (i) out += ", ";
                const auto& fn = s->fieldOrder[i];
                out += fn + "=";
                auto it = s->fields.find(fn);
                if (it != s->fields.end() && it->second.isString())
                    out += "\"" + it->second.asString() + "\"";
                else if (it != s->fields.end())
                    out += it->second.toString();
                else
                    out += "None";
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
        if (isStruct())   return asStruct()->typeName;
        return "?";
    }

} // namespace nova