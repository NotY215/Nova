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
        Value callValue(const Value& callee, const std::vector<Value>& args,
            SourceLocation loc);
        std::shared_ptr<Environment> globals() const { return globals_; }

    private:
        std::shared_ptr<Environment> globals_;
        std::shared_ptr<Environment> env_;

        std::unordered_map<std::string, std::shared_ptr<ClassObject>> classes_;
        std::unordered_map<std::string, const ClassStmt*>             classDecls_;

        void  exec(const Stmt* s);
        void  execBlock(const Block& b);
        Value eval(const Expr* e);
        Value evalBinary(const BinaryExpr* b);
        Value evalAttr(const AttrExpr* a);
        Value evalCall(const CallExpr* c);
        Value evalIndex(const IndexExpr* ix);
        Value evalListLit(const ListLitExpr* n);
        Value evalMapLit(const MapLitExpr* n);
        void  execAssign(const AssignStmt* n);
        void  execFor(const ForStmt* n);

        Value callUser(const std::shared_ptr<Callable>& fn,
            const std::vector<Value>& args,
            SourceLocation loc);
        Value callListMethod(const std::shared_ptr<Callable>& fn,
            const std::vector<Value>& args, SourceLocation loc);
        Value callMapMethod(const std::shared_ptr<Callable>& fn,
            const std::vector<Value>& args, SourceLocation loc);

        void registerStruct(const StructStmt* d);
        void registerClass(const ClassStmt* d);

        Value constructInstance(const std::shared_ptr<ClassObject>& cls,
            const std::vector<std::pair<std::string, Value>>& args,
            SourceLocation loc);

        std::shared_ptr<Callable> findMethod(const std::shared_ptr<ClassObject>& cls,
            const std::string& name,
            std::shared_ptr<ClassObject>* definingClass);

        void installBuiltins();
    };

} // namespace nova