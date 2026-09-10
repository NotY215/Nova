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
        struct Scope {
            std::unordered_map<std::string, TypePtr> vars;
        };

        std::vector<Scope>                       scopes_;
        std::unordered_map<std::string, TypePtr> functions_;
        std::unordered_map<std::string, TypePtr> structs_;
        std::unordered_map<std::string, const StructStmt*> structDecls_;

        TypePtr currentReturnType_;
        int     loopDepth_ = 0;

        void    collectSignatures(const Block& program);   // pass 1
        void    checkStmt(const Stmt* s);
        void    checkBlock(const Block& b);
        TypePtr checkExpr(const Expr* e);

        void    pushScope();
        void    popScope();
        void    defineVar(const std::string& name, TypePtr t);
        TypePtr lookupVar(const std::string& name);

        TypePtr resolveTypeExpr(const Expr* e);

        // struct-constructor checking: Player(name="x", health=1)
        TypePtr checkStructConstruction(const StructStmt* decl,
            const CallExpr* call,
            const std::string& structName);

        [[noreturn]] void error(SourceLocation loc, const std::string& msg);
        void installBuiltins();
    };

} // namespace nova