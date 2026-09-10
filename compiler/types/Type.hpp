#pragma once
#include <memory>
#include <string>
#include <vector>

namespace nova {

    enum class TypeKind {
        None, Bool, Int, Float, Str, Char, Bytes,
        Any,        // wildcard — matches anything (builtins use this)
        Unknown,    // not yet inferred
        Error,      // poisoned after an error, suppresses cascades
        Function,
        Named,      // user types later (struct, class, enum)
    };

    class Type;
    using TypePtr = std::shared_ptr<Type>;

    class Type {
    public:
        TypeKind             kind;
        std::string          name;        // for Named
        std::vector<TypePtr> params;      // function params / generic args
        TypePtr              returnType;  // for Function

        explicit Type(TypeKind k) : kind(k) {}
        Type(TypeKind k, std::string n) : kind(k), name(std::move(n)) {}

        std::string toString() const;
        bool        equals(const TypePtr& other) const;
    };

    namespace Types {
        TypePtr None();
        TypePtr Bool();
        TypePtr Int();
        TypePtr Float();
        TypePtr Str();
        TypePtr Char();
        TypePtr Bytes();
        TypePtr Any();
        TypePtr Unknown();
        TypePtr Error();
        TypePtr Function(std::vector<TypePtr> params, TypePtr ret);
    }

    /// Can a value of type `from` be stored in a slot of type `to`?
    bool isAssignable(const TypePtr& to, const TypePtr& from);

    /// Numeric binary op result type (Error if invalid).
    TypePtr commonNumeric(const TypePtr& a, const TypePtr& b);

} // namespace nova