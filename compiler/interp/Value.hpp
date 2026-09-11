#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace vayu {

    class  Environment;
    struct DefStmt;
    struct LambdaExpr;
    struct Chunk;          // forward decl for VMFunction
    class  Value;
    struct ClassObject;
    struct ListValue;
    struct MapValue;
    struct ModuleValue;
    struct StructInstance;
    struct Callable;

    class Value {
    public:
        using FnPtr = std::shared_ptr<Callable>;
        using InstPtr = std::shared_ptr<StructInstance>;
        using ClassPtr = std::shared_ptr<ClassObject>;
        using ListPtr = std::shared_ptr<ListValue>;
        using MapPtr = std::shared_ptr<MapValue>;
        using ModulePtr = std::shared_ptr<ModuleValue>;

        using Storage = std::variant<
            std::monostate,
            bool, long long, double, std::string,
            FnPtr, InstPtr, ClassPtr, ListPtr, MapPtr, ModulePtr
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
        Value(ModulePtr m) : data_(std::move(m)) {}

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
        bool isModule()   const { return std::holds_alternative<ModulePtr>(data_); }

        bool               asBool()   const { return std::get<bool>(data_); }
        long long          asInt()    const { return std::get<long long>(data_); }
        double             asFloat()  const { return std::get<double>(data_); }
        const std::string& asString() const { return std::get<std::string>(data_); }
        FnPtr     asCallable() const { return std::get<FnPtr>(data_); }
        InstPtr   asInstance() const { return std::get<InstPtr>(data_); }
        ClassPtr  asClass()    const { return std::get<ClassPtr>(data_); }
        ListPtr   asList()     const { return std::get<ListPtr>(data_); }
        MapPtr    asMap()      const { return std::get<MapPtr>(data_); }
        ModulePtr asModule()   const { return std::get<ModulePtr>(data_); }

        double      asDouble() const { return isInt() ? (double)asInt() : asFloat(); }
        bool        truthy()   const;
        std::string toString() const;
        std::string typeName() const;
    };

    struct StructInstance {
        std::shared_ptr<ClassObject>           cls;
        std::unordered_map<std::string, Value> fields;
    };
    struct ListValue { std::vector<Value> items; };
    struct MapValue { std::unordered_map<std::string, Value> entries; };
    struct ModuleValue {
        std::string                            name;
        std::unordered_map<std::string, Value> members;
    };
    struct ClassObject {
        std::string                  name;
        std::vector<std::string>     fieldOrder;
        std::shared_ptr<ClassObject> parent;
    };

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
            StringMethod,
            Lambda,
            VMFunction,       // bytecode-compiled function
        } kind = Kind::Native;

        std::string name;

        NativeFnPtr nativeFn = nullptr;

        // User / BoundMethod / SuperMethod / Lambda
        const DefStmt* decl = nullptr;
        std::shared_ptr<Environment> closure;
        std::shared_ptr<ClassObject> definingClass;
        const LambdaExpr* lambdaExpr = nullptr;

        // VMFunction
        std::shared_ptr<Chunk>       chunk;
        std::vector<std::string>     vmParams;

        // ClassCtor
        std::shared_ptr<ClassObject> classObj;

        // BoundMethod / SuperMethod
        std::shared_ptr<StructInstance> boundSelf;
        std::shared_ptr<Callable>       methodFn;
        std::shared_ptr<ClassObject>    superParent;

        // ListMethod / MapMethod / StringMethod
        std::shared_ptr<ListValue> boundList;
        std::shared_ptr<MapValue>  boundMap;
        std::string                boundStr;
    };

} // namespace vayu