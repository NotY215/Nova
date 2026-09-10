#include "Type.hpp"

namespace nova {

    const StructFieldInfo* Type::findField(const std::string& n) const {
        for (auto& f : fields) if (f.name == n) return &f;
        return nullptr;
    }

    std::string Type::toString() const {
        switch (kind) {
        case TypeKind::None:    return "None";
        case TypeKind::Bool:    return "bool";
        case TypeKind::Int:     return "int";
        case TypeKind::Float:   return "float";
        case TypeKind::Str:     return "str";
        case TypeKind::Char:    return "char";
        case TypeKind::Bytes:   return "bytes";
        case TypeKind::Any:     return "any";
        case TypeKind::Unknown: return "?";
        case TypeKind::Error:   return "<error>";
        case TypeKind::Named:   return name;
        case TypeKind::Struct:  return name;
        case TypeKind::Function: {
            std::string s = "(";
            for (size_t i = 0; i < params.size(); ++i) {
                if (i) s += ", ";
                s += params[i]->toString();
            }
            s += ") -> ";
            s += returnType ? returnType->toString() : "?";
            return s;
        }
        }
        return "?";
    }

    bool Type::equals(const TypePtr& other) const {
        if (!other) return false;
        if (kind != other->kind) return false;
        if (kind == TypeKind::Named || kind == TypeKind::Struct)
            return name == other->name;
        if (kind == TypeKind::Function) {
            if (params.size() != other->params.size()) return false;
            for (size_t i = 0; i < params.size(); ++i)
                if (!params[i]->equals(other->params[i])) return false;
            if (!returnType && !other->returnType) return true;
            if (!returnType || !other->returnType) return false;
            return returnType->equals(other->returnType);
        }
        return true;
    }

    namespace Types {

        static TypePtr make(TypeKind k) { return std::make_shared<Type>(k); }

        TypePtr None() { static auto t = make(TypeKind::None);    return t; }
        TypePtr Bool() { static auto t = make(TypeKind::Bool);    return t; }
        TypePtr Int() { static auto t = make(TypeKind::Int);     return t; }
        TypePtr Float() { static auto t = make(TypeKind::Float);   return t; }
        TypePtr Str() { static auto t = make(TypeKind::Str);     return t; }
        TypePtr Char() { static auto t = make(TypeKind::Char);    return t; }
        TypePtr Bytes() { static auto t = make(TypeKind::Bytes);   return t; }
        TypePtr Any() { static auto t = make(TypeKind::Any);     return t; }
        TypePtr Unknown() { static auto t = make(TypeKind::Unknown); return t; }
        TypePtr Error() { static auto t = make(TypeKind::Error);   return t; }

        TypePtr Function(std::vector<TypePtr> params, TypePtr ret) {
            auto t = std::make_shared<Type>(TypeKind::Function);
            t->params = std::move(params);
            t->returnType = std::move(ret);
            return t;
        }

        TypePtr Struct(std::string name, std::vector<StructFieldInfo> fields) {
            auto t = std::make_shared<Type>(TypeKind::Struct, std::move(name));
            t->fields = std::move(fields);
            return t;
        }

    } // namespace Types

    bool isAssignable(const TypePtr& to, const TypePtr& from) {
        if (!to || !from) return false;
        if (to->kind == TypeKind::Any || from->kind == TypeKind::Any)     return true;
        if (to->kind == TypeKind::Error || from->kind == TypeKind::Error)   return true;
        if (to->kind == TypeKind::Unknown || from->kind == TypeKind::Unknown) return true;
        if (to->kind == TypeKind::Float && from->kind == TypeKind::Int)         return true;
        if (to->kind == TypeKind::Struct && from->kind == TypeKind::Struct)
            return to->name == from->name;
        if (to->kind == TypeKind::Named && from->kind == TypeKind::Named)
            return to->name == from->name;
        return to->equals(from);
    }

    TypePtr commonNumeric(const TypePtr& a, const TypePtr& b) {
        if (a->kind == TypeKind::Error || b->kind == TypeKind::Error) return Types::Error();
        if (a->kind == TypeKind::Any || b->kind == TypeKind::Any)   return Types::Any();
        bool aNum = a->kind == TypeKind::Int || a->kind == TypeKind::Float;
        bool bNum = b->kind == TypeKind::Int || b->kind == TypeKind::Float;
        if (!aNum || !bNum) return Types::Error();
        if (a->kind == TypeKind::Int && b->kind == TypeKind::Int) return Types::Int();
        return Types::Float();
    }

} // namespace nova