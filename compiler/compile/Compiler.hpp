#pragma once
#include "ast/Ast.hpp"
#include "bytecode/Chunk.hpp"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vayu {

    class CompileError : public std::runtime_error {
    public:
        SourceLocation loc;
        CompileError(std::string msg, SourceLocation l)
            : std::runtime_error(std::move(msg)), loc(l) {}
    };

    class Compiler {
    public:
        void compile(const Block& program, Chunk& out);

    private:
        struct ClassInfo {
            std::string              parentName;
            std::vector<std::string> ownFields;
            std::vector<std::string> allFields;
            std::vector<std::string> initParams;
            bool                     hasInit = false;
        };

        struct LoopContext {
            std::vector<size_t> breakJumps;
            size_t              continueTarget = 0;
        };

        Chunk* chunk_ = nullptr;
        std::unordered_map<std::string, ClassInfo> classInfo_;
        std::unordered_map<std::string,
            std::unordered_map<std::string, long long>> enums_;
        // Phase 11.1c: class name -> { static member name -> true }
        std::unordered_map<std::string,
            std::unordered_map<std::string, bool>> statics_;
        std::vector<LoopContext>                   loopStack_;

        // declaration pre-pass
        void collectDeclarations(const Block& program);
        void resolveAllFields();
        std::vector<std::string> resolveFields(const std::string& name,
            std::unordered_set<std::string>& visiting);

        // statements
        void compileStmt(const Stmt* s);
        void compileBlock(const Block& b);
        void compileIf(const IfStmt* n);
        void compileWhile(const WhileStmt* n);
        void compileFor(const ForStmt* n);
        void compileDef(const DefStmt* n);
        void compileReturn(const ReturnStmt* n);
        void compileTry(const TryStmt* n);
        void compileRaise(const RaiseStmt* n);
        void compileImport(const ImportStmt* n);
        void compileFrom(const FromImportStmt* n);

        // expressions
        void compileExpr(const Expr* e);
        void compileAttrGet(const AttrExpr* a, int line);
        void compileCall(const CallExpr* c, int line);
        void compileLambda(const LambdaExpr* n);
        void compileAssignAttr(const AttrExpr* target, const Expr* value, int line);

        // helpers
        size_t emitJump(OpCode op, int line);
        void   patchJump(size_t operandPos, size_t target);
        void   emitLoop(size_t loopStart, int line);
        void   emitJumpTo(size_t target, int line);
        void   emitNameU16(OpCode op, const std::string& name, int line);
        void   emitNameU16WithCount(OpCode op, const std::string& name,
            uint8_t count, int line);

        static std::string staticName(const std::string& cls, const std::string& m) {
            return "__static_" + cls + "__" + m;
        }

        [[noreturn]] void error(SourceLocation loc, const std::string& msg);
    };

} // namespace vayu