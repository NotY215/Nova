#pragma once
#include "Environment.hpp"
#include "Value.hpp"
#include "ast/Ast.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

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

        Value callValue(const Value& callee,
            const std::vector<Value>& args,
            SourceLocation loc);

        std::shared_ptr<Environment> globals() const { return globals_; }

    private:
        std::shared_ptr<Environment> globals_;
        std::shared_ptr<Environment> env_;

        // struct name -> original declaration, so we can construct instances.
        std::unordered_map<std::string, const StructStmt*> structDecls_;

        void  exec(const Stmt* s);
        void  execBlock(const Block& b);

        Value eval(const Expr* e);
        Value evalBinary(const BinaryExpr* b);
        Value evalAttr(const AttrExpr* a);
        Value evalCall(const CallExpr* c);

        void  execAssign(const AssignStmt* n);

        Value callUser(const std::shared_ptr<Callable>& fn,
            const std::vector<Value>& args,
            SourceLocation loc);

        Value constructStruct(const std::string& name,
            const std::vector<std::pair<std::string, Value>>& args,
            SourceLocation loc);

        void installBuiltins();
    };

} // namespace nova