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

        std::vector<Scope>                        scopes_;
        std::unordered_map<std::string, TypePtr>  functions_;

        TypePtr currentReturnType_;   // null outside a function
        int     loopDepth_ = 0;

        // passes
        void collectSignatures(const Block& program);

        // statements / blocks
        void checkStmt(const Stmt* s);
        void checkBlock(const Block& b);

        // expressions
        TypePtr checkExpr(const Expr* e);

        // scopes
        void    pushScope();
        void    popScope();
        void    defineVar(const std::string& name, TypePtr t);
        TypePtr lookupVar(const std::string& name);

        // type-expr → Type
        TypePtr resolveTypeExpr(const Expr* e);

        // diagnostics
        [[noreturn]] void error(SourceLocation loc, const std::string& msg);

        // builtins
        void installBuiltins();
    };

} // namespace nova