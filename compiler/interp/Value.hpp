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

    // ---- instance (struct or class) ----
    struct StructInstance {
        std::shared_ptr<ClassObject>           cls;
        std::unordered_map<std::string, Value> fields;
    };

    // ---- class (or plain struct — methods/parent empty) ----
    struct ClassObject {
        std::string                        name;
        std::vector<std::string>           fieldOrder;   // declared order
        std::shared_ptr<ClassObject>       parent;
    };

    using NativeFnPtr = Value(*)(const std::vector<Value>&);

    struct Callable {
        enum class Kind { Native, User, ClassCtor, BoundMethod, SuperMethod } kind = Kind::Native;
        std::string name;

        // Native
        NativeFnPtr nativeFn = nullptr;

        // User (top-level or method)
        const DefStmt* decl = nullptr;
        std::shared_ptr<Environment> closure;
        std::shared_ptr<ClassObject> definingClass;   // for methods

        // ClassCtor
        std::shared_ptr<ClassObject> classObj;

        // BoundMethod / SuperMethod
        std::shared_ptr<StructInstance> boundSelf;
        std::shared_ptr<Callable>       methodFn;
        std::shared_ptr<ClassObject>    superParent;   // for SuperMethod
    };

    class Value {
        using Storage = std::variant<
            std::monostate, bool, long long, double, std::string,
            std::shared_ptr<Callable>,
            std::shared_ptr<StructInstance>,
            std::shared_ptr<ClassObject>
        >;
        Storage data_;
    public:
        Value() : data_(std::monostate{}) {}
        Value(std::nullptr_t) : data_(std::monostate{}) {}
        Value(bool b) : data_(b) {}
        Value(long long i) : data_(i) {}
        Value(int i) : data_(static_cast<long long>(i)) {}
        Value(double d) : data_(d) {}
        Value(std::string s) : data_(std::move(s)) {}
        Value(const char* s) : data_(std::string(s)) {}
        Value(std::shared_ptr<Callable> c) : data_(std::move(c)) {}
        Value(std::shared_ptr<StructInstance> s) : data_(std::move(s)) {}
        Value(std::shared_ptr<ClassObject> c) : data_(std::move(c)) {}

        bool isNone()     const { return std::holds_alternative<std::monostate>(data_); }
        bool isBool()     const { return std::holds_alternative<bool>(data_); }
        bool isInt()      const { return std::holds_alternative<long long>(data_); }
        bool isFloat()    const { return std::holds_alternative<double>(data_); }
        bool isNumber()   const { return isInt() || isFloat(); }
        bool isString()   const { return std::holds_alternative<std::string>(data_); }
        bool isCallable() const { return std::holds_alternative<std::shared_ptr<Callable>>(data_); }
        bool isInstance() const { return std::holds_alternative<std::shared_ptr<StructInstance>>(data_); }
        bool isClass()    const { return std::holds_alternative<std::shared_ptr<ClassObject>>(data_); }

        bool               asBool()   const { return std::get<bool>(data_); }
        long long          asInt()    const { return std::get<long long>(data_); }
        double             asFloat()  const { return std::get<double>(data_); }
        const std::string& asString() const { return std::get<std::string>(data_); }
        std::shared_ptr<Callable> asCallable() const { return std::get<std::shared_ptr<Callable>>(data_); }
        std::shared_ptr<StructInstance> asInstance() const { return std::get<std::shared_ptr<StructInstance>>(data_); }
        std::shared_ptr<ClassObject>    asClass()    const { return std::get<std::shared_ptr<ClassObject>>(data_); }

        double      asDouble() const { return isInt() ? (double)asInt() : asFloat(); }
        bool        truthy()   const;
        std::string toString() const;
        std::string typeName() const;
    };

} // namespace nova