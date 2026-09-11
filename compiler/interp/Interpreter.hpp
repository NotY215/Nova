#pragma once
#include "Environment.hpp"
#include "Value.hpp"
#include "ast/Ast.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <lexer/Token.hpp>

namespace vayu {

    class RuntimeError : public std::runtime_error {
    public:
        SourceLocation loc;
        RuntimeError(std::string msg, SourceLocation l)
            : std::runtime_error(std::move(msg)), loc(l) {}
    };

    struct VayuException {
        Value          value;
        SourceLocation loc;
    };

    class Interpreter {
    public:
        static Interpreter* current_;

        Interpreter();
        void run(const Block& program);
        Value callValue(const Value& callee, const std::vector<Value>& args,
            SourceLocation loc);
        std::shared_ptr<Environment> globals() const { return globals_; }
        /// Register top-level struct / class declarations without executing
        /// the program.  Called by the driver before running on the VM.
        void registerDeclarations(const Block& program);

        // ---- Phase 4C: hooks for the bytecode VM ----
        // These mirror the logic of the tree-walking evaluator's attr lookup,
        // instance construction, and method dispatch, but operate on
        // pre-evaluated values.
        Value vmGetAttr(const Value& base, const std::string& name, SourceLocation loc);
        void  vmSetAttr(const Value& base, const std::string& name,
            const Value& v, SourceLocation loc);
        Value vmNewInst(const std::string& className,
            const std::vector<Value>& args, SourceLocation loc);

        /// Directory to search first when resolving module imports.
        /// Should include a trailing separator.  Empty means "current directory".
        void setSourceDir(const std::string& dir) { sourceDir_ = dir; }

        static std::string exceptionTypeName(const Value& v);
        static std::string exceptionMessage(const Value& v);

    private:
        std::shared_ptr<Environment> globals_;
        std::shared_ptr<Environment> env_;
        std::string                  sourceDir_;

        std::unordered_map<std::string, std::shared_ptr<ClassObject>> classes_;
        std::unordered_map<std::string, const ClassStmt*>             classDecls_;

        std::unordered_map<std::string, std::shared_ptr<ModuleValue>> moduleCache_;
        // Persists module ASTs so that DefStmt / ClassStmt pointers held by
        // Callables and classDecls_ stay valid for the interpreter's lifetime.
        std::vector<std::unique_ptr<Block>> moduleAsts_;

        std::vector<Value> activeExceptions_;

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
        void  execTry(const TryStmt* t);
        void  execRaise(const RaiseStmt* r);
        void  execImport(const ImportStmt* n);
        void  execFromImport(const FromImportStmt* n);

        Value loadModule(const std::string& name, SourceLocation loc);
        bool  findModuleFile(const std::string& name, std::string& pathOut) const;

        Value callUser(const std::shared_ptr<Callable>& fn,
            const std::vector<Value>& args, SourceLocation loc);
        Value callLambda(const std::shared_ptr<Callable>& fn,
            const std::vector<Value>& args, SourceLocation loc);
        Value callListMethod(const std::shared_ptr<Callable>& fn,
            const std::vector<Value>& args, SourceLocation loc);
        Value callMapMethod(const std::shared_ptr<Callable>& fn,
            const std::vector<Value>& args, SourceLocation loc);
        Value callStringMethod(const std::shared_ptr<Callable>& fn,
            const std::vector<Value>& args, SourceLocation loc);

        void registerStruct(const StructStmt* d);
        void registerClass(const ClassStmt* d);

        Value constructInstance(const std::shared_ptr<ClassObject>& cls,
            const std::vector<std::pair<std::string, Value>>& args,
            SourceLocation loc);

        std::shared_ptr<Callable> findMethod(const std::shared_ptr<ClassObject>& cls,
            const std::string& name,
            std::shared_ptr<ClassObject>* definingClass);

        bool  valueIsInstanceOf(const Value& v,
            const std::shared_ptr<ClassObject>& cls) const;
        Value makeException(const std::string& typeName, const std::string& msg);

        void installBuiltins();
        void installMathModule();
        void installExceptionClasses();
    };

} // namespace vayu