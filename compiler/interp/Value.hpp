#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace nova {

    class  Environment;
    struct DefStmt;
    class  Value;

    // Native C++ function exposed to Nova scripts.
    using NativeFnPtr = Value(*)(const std::vector<Value>&);

    // One uniform Callable covers both native builtins and user-defined `def`s.
    //   - Native:  nativeFn != nullptr
    //   - User:    decl != nullptr, closure holds the captured environment
    struct Callable {
        enum class Kind { Native, User } kind = Kind::Native;
        std::string                  name;
        NativeFnPtr                  nativeFn = nullptr;
        const DefStmt* decl = nullptr;
        std::shared_ptr<Environment> closure;
    };

    class Value {
        using Storage = std::variant<
            std::monostate,                 // None
            bool,
            long long,
            double,
            std::string,
            std::shared_ptr<Callable>
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

        // ---- type queries ----
        bool isNone()     const { return std::holds_alternative<std::monostate>(data_); }
        bool isBool()     const { return std::holds_alternative<bool>(data_); }
        bool isInt()      const { return std::holds_alternative<long long>(data_); }
        bool isFloat()    const { return std::holds_alternative<double>(data_); }
        bool isNumber()   const { return isInt() || isFloat(); }
        bool isString()   const { return std::holds_alternative<std::string>(data_); }
        bool isCallable() const { return std::holds_alternative<std::shared_ptr<Callable>>(data_); }

        // ---- accessors (caller must check type first) ----
        bool               asBool()   const { return std::get<bool>(data_); }
        long long          asInt()    const { return std::get<long long>(data_); }
        double             asFloat()  const { return std::get<double>(data_); }
        const std::string& asString() const { return std::get<std::string>(data_); }
        std::shared_ptr<Callable> asCallable() const {
            return std::get<std::shared_ptr<Callable>>(data_);
        }

        // ---- helpers ----
        double asDouble() const { return isInt() ? (double)asInt() : asFloat(); }
        bool   truthy()   const;
        std::string toString() const;
        std::string typeName() const;
    };

} // namespace nova