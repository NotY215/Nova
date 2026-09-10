#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nova {

    enum class TypeKind {
        None, Bool, Int, Float, Str, Char, Bytes,
        Any, Unknown, Error,
        Function,
        Struct,
        Named,
    };

    class Type;
    using TypePtr = std::shared_ptr<Type>;

    // One declared struct field
    struct StructFieldInfo {
        std::string name;
        TypePtr     type;
    };

    class Type {
    public:
        TypeKind             kind;
        std::string          name;
        std::vector<TypePtr> params;
        TypePtr              returnType;

        // for Struct
        std::vector<StructFieldInfo> fields;

        explicit Type(TypeKind k) : kind(k) {}
        Type(TypeKind k, std::string n) : kind(k), name(std::move(n)) {}

        std::string toString() const;
        bool        equals(const TypePtr& other) const;

        /// For Struct kinds only. Returns nullptr if field not found.
        const StructFieldInfo* findField(const std::string& n) const;
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
        TypePtr Struct(std::string name, std::vector<StructFieldInfo> fields);
    }

    bool    isAssignable(const TypePtr& to, const TypePtr& from);
    TypePtr commonNumeric(const TypePtr& a, const TypePtr& b);

} // namespace nova