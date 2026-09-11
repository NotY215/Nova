#pragma once
#include "types/Type.hpp"
#include "ast/Ast.hpp"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace nova {

    class TypeError : public std::runtime_error {
    public:
        SourceLocation loc;
        TypeError(std::string msg, SourceLocation l)
            : std::runtime_error(std::move(msg)), loc(l) {}
    };

    class TypeChecker {
    public:
        void check(const Block& program);

    private:
        struct Scope { std::unordered_map<std::string, TypePtr> vars; };

        std::vector<Scope>                       scopes_;
        std::unordered_map<std::string, TypePtr> functions_;
        std::unordered_map<std::string, TypePtr> structs_;

        TypePtr currentReturnType_;
        TypePtr currentClass_;
        int     loopDepth_ = 0;

        void    collectSignatures(const Block& program);
        void    checkStmt(const Stmt* s);
        void    checkBlock(const Block& b);
        TypePtr checkExpr(const Expr* e);

        void    pushScope();
        void    popScope();
        void    defineVar(const std::string& name, TypePtr t);
        TypePtr lookupVar(const std::string& name);

        TypePtr resolveTypeExpr(const Expr* e);

        TypePtr checkStructConstruction(const StructStmt* decl, const CallExpr* call,
            const std::string& name);
        void    checkMethodBody(const DefStmt* m, TypePtr cls);

        // collection helpers
        TypePtr lookupCollectionMethod(const TypePtr& target, const std::string& name,
            SourceLocation loc);
        TypePtr commonElementType(const TypePtr& a, const TypePtr& b, SourceLocation loc);

        [[noreturn]] void error(SourceLocation loc, const std::string& msg);
        void installBuiltins();
        void installBuiltinExceptions();
    };

} // namespace nova