#pragma once
#include "Environment.hpp"
#include "Value.hpp"
#include "ast/Ast.hpp"
#include <memory>
#include <stdexcept>
#include <string>

namespace nova {

    class RuntimeError : public std::runtime_error {
    public:
        SourceLocation loc;
        RuntimeError(std::string msg, SourceLocation l)
            : std::runtime_error(std::move(msg)), loc(l) {}
    };

    class Interpreter {
    public:
        Interpreter();

        void run(const Block& program);

        // Used by native built-ins that need to call back into Nova.
        Value callValue(const Value& callee, const std::vector<Value>& args,
            SourceLocation loc);

        std::shared_ptr<Environment> globals() const { return globals_; }

    private:
        std::shared_ptr<Environment> globals_;
        std::shared_ptr<Environment> env_;

        // ---- statements ----
        void  exec(const Stmt* s);
        void  execBlock(const Block& b);

        // ---- expressions ----
        Value eval(const Expr* e);
        Value evalBinary(const BinaryExpr* b);

        // ---- calls ----
        Value callUser(const std::shared_ptr<Callable>& fn,
            const std::vector<Value>& args,
            SourceLocation loc);

        void installBuiltins();
    };

} // namespace nova