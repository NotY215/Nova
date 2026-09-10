#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace nova {

    class  Environment;
    struct DefStmt;
    class  Value;
    struct ClassObject;
    struct ListValue;
    struct MapValue;
    struct StructInstance;
    struct Callable;

    // ===========================================================================
    // Value  (tagged union over all runtime types)
    // ===========================================================================

    class Value {
    public:
        using FnPtr = std::shared_ptr<Callable>;
        using InstPtr = std::shared_ptr<StructInstance>;
        using ClassPtr = std::shared_ptr<ClassObject>;
        using ListPtr = std::shared_ptr<ListValue>;
        using MapPtr = std::shared_ptr<MapValue>;

        using Storage = std::variant<
            std::monostate,
            bool, long long, double, std::string,
            FnPtr, InstPtr, ClassPtr, ListPtr, MapPtr
        >;
        Storage data_;

        Value() : data_(std::monostate{}) {}
        Value(std::nullptr_t) : data_(std::monostate{}) {}
        Value(bool b) : data_(b) {}
        Value(long long i) : data_(i) {}
        Value(int i) : data_(static_cast<long long>(i)) {}
        Value(double d) : data_(d) {}
        Value(std::string s) : data_(std::move(s)) {}
        Value(const char* s) : data_(std::string(s)) {}
        Value(FnPtr   c) : data_(std::move(c)) {}
        Value(InstPtr s) : data_(std::move(s)) {}
        Value(ClassPtr c) : data_(std::move(c)) {}
        Value(ListPtr l) : data_(std::move(l)) {}
        Value(MapPtr  m) : data_(std::move(m)) {}

        bool isNone()     const { return std::holds_alternative<std::monostate>(data_); }
        bool isBool()     const { return std::holds_alternative<bool>(data_); }
        bool isInt()      const { return std::holds_alternative<long long>(data_); }
        bool isFloat()    const { return std::holds_alternative<double>(data_); }
        bool isNumber()   const { return isInt() || isFloat(); }
        bool isString()   const { return std::holds_alternative<std::string>(data_); }
        bool isCallable() const { return std::holds_alternative<FnPtr>(data_); }
        bool isInstance() const { return std::holds_alternative<InstPtr>(data_); }
        bool isClass()    const { return std::holds_alternative<ClassPtr>(data_); }
        bool isList()     const { return std::holds_alternative<ListPtr>(data_); }
        bool isMap()      const { return std::holds_alternative<MapPtr>(data_); }

        bool               asBool()   const { return std::get<bool>(data_); }
        long long          asInt()    const { return std::get<long long>(data_); }
        double             asFloat()  const { return std::get<double>(data_); }
        const std::string& asString() const { return std::get<std::string>(data_); }
        FnPtr    asCallable() const { return std::get<FnPtr>(data_); }
        InstPtr  asInstance() const { return std::get<InstPtr>(data_); }
        ClassPtr asClass()    const { return std::get<ClassPtr>(data_); }
        ListPtr  asList()     const { return std::get<ListPtr>(data_); }
        MapPtr   asMap()      const { return std::get<MapPtr>(data_); }

        double      asDouble() const { return isInt() ? (double)asInt() : asFloat(); }
        bool        truthy()   const;
        std::string toString() const;
        std::string typeName() const;
    };

    // ===========================================================================
    // Runtime object types
    // ===========================================================================

    struct StructInstance {
        std::shared_ptr<ClassObject>           cls;
        std::unordered_map<std::string, Value> fields;
    };

    struct ListValue {
        std::vector<Value> items;
    };

    struct MapValue {
        std::unordered_map<std::string, Value> entries;
    };

    struct ClassObject {
        std::string                  name;
        std::vector<std::string>     fieldOrder;
        std::shared_ptr<ClassObject> parent;
    };

    // ===========================================================================
    // Callables (native builtins, user defs, bound methods, class ctors,
    //            list methods, map methods)
    // ===========================================================================

    using NativeFnPtr = Value(*)(const std::vector<Value>&);

    struct Callable {
        enum class Kind {
            Native,
            User,
            ClassCtor,
            BoundMethod,
            SuperMethod,
            ListMethod,
            MapMethod,
        } kind = Kind::Native;

        std::string name;

        // Native
        NativeFnPtr nativeFn = nullptr;

        // User / BoundMethod / SuperMethod
        const DefStmt* decl = nullptr;
        std::shared_ptr<Environment> closure;
        std::shared_ptr<ClassObject> definingClass;

        // ClassCtor
        std::shared_ptr<ClassObject> classObj;

        // BoundMethod / SuperMethod
        std::shared_ptr<StructInstance> boundSelf;
        std::shared_ptr<Callable>       methodFn;
        std::shared_ptr<ClassObject>    superParent;

        // ListMethod / MapMethod
        std::shared_ptr<ListValue> boundList;
        std::shared_ptr<MapValue>  boundMap;
    };

} // namespace nova