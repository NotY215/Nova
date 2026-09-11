#pragma once
#include "ast/Ast.hpp"
#include "bytecode/Chunk.hpp"
#include <stdexcept>
#include <string>

namespace nova {

    class CompileError : public std::runtime_error {
    public:
        SourceLocation loc;
        CompileError(std::string msg, SourceLocation l)
            : std::runtime_error(std::move(msg)), loc(l) {}
    };

    class Compiler {
    public:
        /// Compile a program to bytecode. Throws CompileError on unsupported AST.
        void compile(const Block& program, Chunk& out);

    private:
        Chunk* chunk_ = nullptr;

        void compileStmt(const Stmt* s);
        void compileBlock(const Block& b);
        void compileExpr(const Expr* e);

        void compileIf(const IfStmt* n);
        void compileWhile(const WhileStmt* n);
        void compileFor(const ForStmt* n);

        size_t emitJump(OpCode op, int line);
        void   patchJump(size_t operandPos, size_t target);
        void   emitLoop(size_t loopStart, int line);

        [[noreturn]] void error(SourceLocation loc, const std::string& msg);
    };

} // namespace nova