#include "Compiler.hpp"

namespace vayu {

    void Compiler::compile(const Block& program, Chunk& out) {
        chunk_ = &out;
        compileBlock(program);
        chunk_->emitOp(OpCode::NONE, 0);
        chunk_->emitOp(OpCode::RETURN_V, 0);
        chunk_ = nullptr;
    }

    [[noreturn]] void Compiler::error(SourceLocation loc, const std::string& msg) {
        throw CompileError(msg, loc);
    }

    size_t Compiler::emitJump(OpCode op, int line) {
        chunk_->emitOp(op, line);
        size_t pos = chunk_->code.size();
        chunk_->emit(0, line);
        chunk_->emit(0, line);
        return pos;
    }
    void Compiler::patchJump(size_t operandPos, size_t target) {
        chunk_->patchJump(operandPos, (int)target, 0);
    }
    void Compiler::emitLoop(size_t loopStart, int line) {
        chunk_->emitOp(OpCode::JUMP, line);
        size_t pos = chunk_->code.size();
        chunk_->emit(0, line);
        chunk_->emit(0, line);
        chunk_->patchJump(pos, (int)loopStart, line);
    }

    // ---------------------------------------------------------------------------
    // Statements
    // ---------------------------------------------------------------------------

    void Compiler::compileBlock(const Block& b) {
        for (auto& s : b.stmts) compileStmt(s.get());
    }

    void Compiler::compileStmt(const Stmt* s) {
        if (!s) return;
        int line = s->loc.line;

        switch (s->kind) {
        case StmtKind::Expr: {
            auto* n = static_cast<const ExprStmt*>(s);
            compileExpr(n->expr.get());
            chunk_->emitOp(OpCode::POP, line);
            return;
        }

        case StmtKind::Assign: {
            auto* n = static_cast<const AssignStmt*>(s);
            if (n->target->kind != ExprKind::NameRef)
                error(n->target->loc,
                    "VM mode: only simple-name assignment is supported in 4B");
            const auto* nm = static_cast<const NameRefExpr*>(n->target.get());
            compileExpr(n->value.get());
            chunk_->emitOp(OpCode::DEFINE, line);
            int idx = chunk_->addName(nm->name);
            chunk_->emit((uint8_t)((idx >> 8) & 0xFF), line);
            chunk_->emit((uint8_t)(idx & 0xFF), line);
            return;
        }

        case StmtKind::AnnotAssign: {
            auto* n = static_cast<const AnnotAssignStmt*>(s);
            if (n->value) compileExpr(n->value.get());
            else          chunk_->emitOp(OpCode::NONE, line);
            chunk_->emitOp(OpCode::DEFINE, line);
            int idx = chunk_->addName(n->name);
            chunk_->emit((uint8_t)((idx >> 8) & 0xFF), line);
            chunk_->emit((uint8_t)(idx & 0xFF), line);
            return;
        }

        case StmtKind::If:     compileIf(static_cast<const IfStmt*>(s));     return;
        case StmtKind::While:  compileWhile(static_cast<const WhileStmt*>(s));  return;
        case StmtKind::For:    compileFor(static_cast<const ForStmt*>(s));    return;
        case StmtKind::Def:    compileDef(static_cast<const DefStmt*>(s));    return;
        case StmtKind::Return: compileReturn(static_cast<const ReturnStmt*>(s)); return;

        case StmtKind::Pass:
            return;

        case StmtKind::Break:
            error(s->loc, "VM mode: break not yet implemented");

        case StmtKind::Continue:
            error(s->loc, "VM mode: continue not yet implemented");

        case StmtKind::Struct:
        case StmtKind::Class:
            error(s->loc, "VM mode: classes and structs arrive in 4E");

        case StmtKind::Try:
        case StmtKind::Raise:
            error(s->loc, "VM mode: exceptions arrive in 4G");

        case StmtKind::Import:
        case StmtKind::FromImport:
            error(s->loc, "VM mode: modules arrive in 4H");
        }
    }

    // ---------------------------------------------------------------------------
    // Functions
    // ---------------------------------------------------------------------------

    void Compiler::compileDef(const DefStmt* n) {
        int line = n->loc.line;

        auto fnChunk = std::make_shared<Chunk>();
        for (auto& p : n->params) fnChunk->paramNames.push_back(p.name);

        Chunk* saved = chunk_;
        chunk_ = fnChunk.get();

        for (auto& st : n->body.stmts) compileStmt(st.get());

        // Implicit fallthrough return
        chunk_->emitOp(OpCode::NONE, line);
        chunk_->emitOp(OpCode::RETURN_V, line);

        chunk_ = saved;

        int fnIdx = chunk_->addFunction(fnChunk);
        chunk_->emitOp(OpCode::MAKE_FN, line);
        chunk_->emit((uint8_t)((fnIdx >> 8) & 0xFF), line);
        chunk_->emit((uint8_t)(fnIdx & 0xFF), line);

        // Bind to name in current scope
        chunk_->emitOp(OpCode::DEFINE, line);
        int nameIdx = chunk_->addName(n->name);
        chunk_->emit((uint8_t)((nameIdx >> 8) & 0xFF), line);
        chunk_->emit((uint8_t)(nameIdx & 0xFF), line);
    }

    void Compiler::compileReturn(const ReturnStmt* n) {
        int line = n->loc.line;
        if (n->value) compileExpr(n->value.get());
        else          chunk_->emitOp(OpCode::NONE, line);
        chunk_->emitOp(OpCode::RETURN_V, line);
    }

    // ---------------------------------------------------------------------------
    // Control flow
    // ---------------------------------------------------------------------------

    void Compiler::compileIf(const IfStmt* n) {
        int line = n->loc.line;
        std::vector<size_t> endJumps;

        compileExpr(n->cond.get());
        size_t skipThen = emitJump(OpCode::JUMP_IF_FALSE, line);
        compileBlock(n->thenBody);
        endJumps.push_back(emitJump(OpCode::JUMP, line));
        patchJump(skipThen, chunk_->here());

        for (auto& ec : n->elifs) {
            compileExpr(ec.cond.get());
            size_t s = emitJump(OpCode::JUMP_IF_FALSE, line);
            compileBlock(ec.body);
            endJumps.push_back(emitJump(OpCode::JUMP, line));
            patchJump(s, chunk_->here());
        }

        if (n->elseBody) compileBlock(*n->elseBody);

        size_t end = chunk_->here();
        for (size_t p : endJumps) patchJump(p, end);
    }

    void Compiler::compileWhile(const WhileStmt* n) {
        int line = n->loc.line;
        size_t loopStart = chunk_->here();

        compileExpr(n->cond.get());
        size_t exitJump = emitJump(OpCode::JUMP_IF_FALSE, line);
        compileBlock(n->body);
        emitLoop(loopStart, line);
        patchJump(exitJump, chunk_->here());
    }

    void Compiler::compileFor(const ForStmt* n) {
        int line = n->loc.line;
        compileExpr(n->iterable.get());
        chunk_->emitOp(OpCode::ITER_NEW, line);

        size_t loopStart = chunk_->here();
        chunk_->emitOp(OpCode::ITER_NEXT, line);
        size_t iterOperandPos = chunk_->code.size();
        chunk_->emit(0, line);
        chunk_->emit(0, line);

        chunk_->emitOp(OpCode::DEFINE, line);
        int nameIdx = chunk_->addName(n->targetName);
        chunk_->emit((uint8_t)((nameIdx >> 8) & 0xFF), line);
        chunk_->emit((uint8_t)(nameIdx & 0xFF), line);

        compileBlock(n->body);
        emitLoop(loopStart, line);
        patchJump(iterOperandPos, chunk_->here());
    }

    // ---------------------------------------------------------------------------
    // Expressions
    // ---------------------------------------------------------------------------

    void Compiler::compileExpr(const Expr* e) {
        if (!e) return;
        int line = e->loc.line;

        switch (e->kind) {
        case ExprKind::IntLit: {
            auto* n = static_cast<const IntLitExpr*>(e);
            int idx = chunk_->addConstant(Value((long long)n->value));
            chunk_->emitOp(OpCode::CONST, line);
            chunk_->emit((uint8_t)((idx >> 8) & 0xFF), line);
            chunk_->emit((uint8_t)(idx & 0xFF), line);
            return;
        }
        case ExprKind::FloatLit: {
            auto* n = static_cast<const FloatLitExpr*>(e);
            int idx = chunk_->addConstant(Value(n->value));
            chunk_->emitOp(OpCode::CONST, line);
            chunk_->emit((uint8_t)((idx >> 8) & 0xFF), line);
            chunk_->emit((uint8_t)(idx & 0xFF), line);
            return;
        }
        case ExprKind::StringLit: {
            auto* n = static_cast<const StringLitExpr*>(e);
            int idx = chunk_->addConstant(Value(n->value));
            chunk_->emitOp(OpCode::CONST, line);
            chunk_->emit((uint8_t)((idx >> 8) & 0xFF), line);
            chunk_->emit((uint8_t)(idx & 0xFF), line);
            return;
        }
        case ExprKind::BoolLit: {
            auto* n = static_cast<const BoolLitExpr*>(e);
            chunk_->emitOp(n->value ? OpCode::TRUE_V : OpCode::FALSE_V, line);
            return;
        }
        case ExprKind::NoneLit:
            chunk_->emitOp(OpCode::NONE, line);
            return;

        case ExprKind::NameRef: {
            auto* n = static_cast<const NameRefExpr*>(e);
            int idx = chunk_->addName(n->name);
            chunk_->emitOp(OpCode::LOAD, line);
            chunk_->emit((uint8_t)((idx >> 8) & 0xFF), line);
            chunk_->emit((uint8_t)(idx & 0xFF), line);
            return;
        }

        case ExprKind::Grouping:
            compileExpr(static_cast<const GroupingExpr*>(e)->inner.get());
            return;

        case ExprKind::Unary: {
            auto* n = static_cast<const UnaryExpr*>(e);
            compileExpr(n->operand.get());
            switch (n->op) {
            case UnOp::Neg: chunk_->emitOp(OpCode::NEG, line); break;
            case UnOp::Pos: break;
            case UnOp::Not: chunk_->emitOp(OpCode::NOT, line); break;
            }
            return;
        }

        case ExprKind::Binary: {
            auto* n = static_cast<const BinaryExpr*>(e);

            if (n->op == BinOp::And) {
                compileExpr(n->lhs.get());
                chunk_->emitOp(OpCode::DUP, line);
                size_t j = emitJump(OpCode::JUMP_IF_FALSE, line);
                chunk_->emitOp(OpCode::POP, line);
                compileExpr(n->rhs.get());
                patchJump(j, chunk_->here());
                return;
            }
            if (n->op == BinOp::Or) {
                compileExpr(n->lhs.get());
                chunk_->emitOp(OpCode::DUP, line);
                size_t j = emitJump(OpCode::JUMP_IF_TRUE, line);
                chunk_->emitOp(OpCode::POP, line);
                compileExpr(n->rhs.get());
                patchJump(j, chunk_->here());
                return;
            }

            compileExpr(n->lhs.get());
            compileExpr(n->rhs.get());
            switch (n->op) {
            case BinOp::Add:      chunk_->emitOp(OpCode::ADD, line); break;
            case BinOp::Sub:      chunk_->emitOp(OpCode::SUB, line); break;
            case BinOp::Mul:      chunk_->emitOp(OpCode::MUL, line); break;
            case BinOp::Div:      chunk_->emitOp(OpCode::DIV, line); break;
            case BinOp::FloorDiv: chunk_->emitOp(OpCode::FLOORDIV, line); break;
            case BinOp::Mod:      chunk_->emitOp(OpCode::MOD, line); break;
            case BinOp::Pow:      chunk_->emitOp(OpCode::POW, line); break;
            case BinOp::Eq:       chunk_->emitOp(OpCode::EQ, line); break;
            case BinOp::NotEq:    chunk_->emitOp(OpCode::NEQ, line); break;
            case BinOp::Lt:       chunk_->emitOp(OpCode::LT, line); break;
            case BinOp::Gt:       chunk_->emitOp(OpCode::GT, line); break;
            case BinOp::LtEq:     chunk_->emitOp(OpCode::LE, line); break;
            case BinOp::GtEq:     chunk_->emitOp(OpCode::GE, line); break;
            case BinOp::In:
            case BinOp::Is:
                error(e->loc, std::string("VM mode: operator '") +
                    binOpName(n->op) + "' not yet implemented");
            case BinOp::And:
            case BinOp::Or:
                return;
            }
            return;
        }

        case ExprKind::Call: {
            auto* n = static_cast<const CallExpr*>(e);
            if (n->callee->kind != ExprKind::NameRef &&
                n->callee->kind != ExprKind::Attr)
                error(e->loc, "VM mode: only plain-name calls are supported in 4B");
            if (n->callee->kind == ExprKind::Attr)
                error(e->loc, "VM mode: method calls arrive in 4E/4F");

            // [callee, arg0, arg1, ..., argN-1]
            compileExpr(n->callee.get());
            for (auto& a : n->args) {
                if (!a.name.empty())
                    error(a.loc, "VM mode: keyword arguments not supported");
                compileExpr(a.value.get());
            }
            chunk_->emitOp(OpCode::CALL, line);
            chunk_->emit((uint8_t)n->args.size(), line);
            return;
        }

        case ExprKind::ListLit: {
            auto* n = static_cast<const ListLitExpr*>(e);
            for (auto& el : n->elements) compileExpr(el.get());
            chunk_->emitOp(OpCode::LIST_NEW, line);
            int cnt = (int)n->elements.size();
            chunk_->emit((uint8_t)((cnt >> 8) & 0xFF), line);
            chunk_->emit((uint8_t)(cnt & 0xFF), line);
            return;
        }

        case ExprKind::Attr:
        case ExprKind::Index:
        case ExprKind::MapLit:
        case ExprKind::Lambda:
        case ExprKind::GenericType:
            error(e->loc, "VM mode: this expression is not yet supported");
        }
    }

} // namespace vayu