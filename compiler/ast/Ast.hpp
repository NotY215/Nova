#pragma once
#include "lexer/Token.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nova {

    // ===========================================================================
    // Expressions
    // ===========================================================================

    enum class ExprKind {
        IntLit, FloatLit, StringLit, CharLit, BoolLit, NoneLit,
        NameRef,
        Unary, Binary, Grouping,
        Call, Attr, Index,
    };

    enum class BinOp {
        Add, Sub, Mul, Div, FloorDiv, Mod, Pow,
        Eq, NotEq, Lt, Gt, LtEq, GtEq,
        And, Or, In, Is,
    };

    enum class UnOp { Neg, Pos, Not };

    struct Expr {
        ExprKind       kind;
        SourceLocation loc;
        Expr(ExprKind k, SourceLocation l) : kind(k), loc(l) {}
        virtual ~Expr() = default;
    };
    using ExprPtr = std::unique_ptr<Expr>;

    struct IntLitExpr : Expr {
        long long   value;
        std::string text;
        IntLitExpr(long long v, std::string t, SourceLocation l)
            : Expr(ExprKind::IntLit, l), value(v), text(std::move(t)) {
        }
    };

    struct FloatLitExpr : Expr {
        double      value;
        std::string text;
        FloatLitExpr(double v, std::string t, SourceLocation l)
            : Expr(ExprKind::FloatLit, l), value(v), text(std::move(t)) {
        }
    };

    struct StringLitExpr : Expr {
        std::string value;
        StringLitExpr(std::string v, SourceLocation l)
            : Expr(ExprKind::StringLit, l), value(std::move(v)) {
        }
    };

    struct CharLitExpr : Expr {
        std::string value;
        CharLitExpr(std::string v, SourceLocation l)
            : Expr(ExprKind::CharLit, l), value(std::move(v)) {
        }
    };

    struct BoolLitExpr : Expr {
        bool value;
        BoolLitExpr(bool v, SourceLocation l)
            : Expr(ExprKind::BoolLit, l), value(v) {
        }
    };

    struct NoneLitExpr : Expr {
        NoneLitExpr(SourceLocation l) : Expr(ExprKind::NoneLit, l) {}
    };

    struct NameRefExpr : Expr {
        std::string name;
        NameRefExpr(std::string n, SourceLocation l)
            : Expr(ExprKind::NameRef, l), name(std::move(n)) {
        }
    };

    struct UnaryExpr : Expr {
        UnOp    op;
        ExprPtr operand;
        UnaryExpr(UnOp o, ExprPtr e, SourceLocation l)
            : Expr(ExprKind::Unary, l), op(o), operand(std::move(e)) {
        }
    };

    struct BinaryExpr : Expr {
        BinOp   op;
        ExprPtr lhs, rhs;
        BinaryExpr(BinOp o, ExprPtr a, ExprPtr b, SourceLocation l)
            : Expr(ExprKind::Binary, l), op(o), lhs(std::move(a)), rhs(std::move(b)) {
        }
    };

    struct GroupingExpr : Expr {
        ExprPtr inner;
        GroupingExpr(ExprPtr e, SourceLocation l)
            : Expr(ExprKind::Grouping, l), inner(std::move(e)) {
        }
    };

    struct CallExpr : Expr {
        ExprPtr              callee;
        std::vector<ExprPtr> args;
        CallExpr(ExprPtr c, std::vector<ExprPtr> a, SourceLocation l)
            : Expr(ExprKind::Call, l), callee(std::move(c)), args(std::move(a)) {
        }
    };

    struct AttrExpr : Expr {
        ExprPtr     target;
        std::string name;
        AttrExpr(ExprPtr t, std::string n, SourceLocation l)
            : Expr(ExprKind::Attr, l), target(std::move(t)), name(std::move(n)) {
        }
    };

    struct IndexExpr : Expr {
        ExprPtr target;
        ExprPtr index;
        IndexExpr(ExprPtr t, ExprPtr i, SourceLocation l)
            : Expr(ExprKind::Index, l), target(std::move(t)), index(std::move(i)) {
        }
    };

    // ===========================================================================
    // Statements
    // ===========================================================================

    struct Stmt;
    using StmtPtr = std::unique_ptr<Stmt>;

    /// A run of statements. Not polymorphic — just a container.
    struct Block {
        std::vector<StmtPtr> stmts;
    };

    enum class StmtKind {
        Expr, Assign, AnnotAssign,
        If, While, Def, Return,
        Pass, Break, Continue,
    };

    struct Stmt {
        StmtKind       kind;
        SourceLocation loc;
        Stmt(StmtKind k, SourceLocation l) : kind(k), loc(l) {}
        virtual ~Stmt() = default;
    };

    struct ExprStmt : Stmt {
        ExprPtr expr;
        ExprStmt(ExprPtr e, SourceLocation l)
            : Stmt(StmtKind::Expr, l), expr(std::move(e)) {
        }
    };

    struct AssignStmt : Stmt {
        ExprPtr target;   // NameRefExpr, AttrExpr, or IndexExpr (validated later)
        ExprPtr value;
        AssignStmt(ExprPtr t, ExprPtr v, SourceLocation l)
            : Stmt(StmtKind::Assign, l), target(std::move(t)), value(std::move(v)) {
        }
    };

    struct AnnotAssignStmt : Stmt {
        std::string name;
        ExprPtr     type;    // parsed as expression for now
        ExprPtr     value;   // may be null: `age: int` alone
        AnnotAssignStmt(std::string n, ExprPtr t, ExprPtr v, SourceLocation l)
            : Stmt(StmtKind::AnnotAssign, l),
            name(std::move(n)), type(std::move(t)), value(std::move(v)) {
        }
    };

    struct ElifClause {
        ExprPtr cond;
        Block   body;
    };

    struct IfStmt : Stmt {
        ExprPtr                    cond;
        Block                      thenBody;
        std::vector<ElifClause>    elifs;
        std::optional<Block>       elseBody;
        IfStmt(ExprPtr c, Block t, SourceLocation l)
            : Stmt(StmtKind::If, l), cond(std::move(c)), thenBody(std::move(t)) {
        }
    };

    struct WhileStmt : Stmt {
        ExprPtr cond;
        Block   body;
        WhileStmt(ExprPtr c, Block b, SourceLocation l)
            : Stmt(StmtKind::While, l), cond(std::move(c)), body(std::move(b)) {
        }
    };

    struct Param {
        std::string name;
        ExprPtr     type;   // may be null
    };

    struct DefStmt : Stmt {
        std::string          name;
        std::vector<Param>   params;
        ExprPtr              returnType;   // may be null
        Block                body;
        DefStmt(std::string n, std::vector<Param> p, ExprPtr rt, Block b, SourceLocation l)
            : Stmt(StmtKind::Def, l),
            name(std::move(n)),
            params(std::move(p)),
            returnType(std::move(rt)),
            body(std::move(b)) {
        }
    };

    struct ReturnStmt : Stmt {
        ExprPtr value;   // may be null: bare `return`
        ReturnStmt(ExprPtr v, SourceLocation l)
            : Stmt(StmtKind::Return, l), value(std::move(v)) {
        }
    };

    struct PassStmt : Stmt {
        PassStmt(SourceLocation l) : Stmt(StmtKind::Pass, l) {}
    };
    struct BreakStmt : Stmt {
        BreakStmt(SourceLocation l) : Stmt(StmtKind::Break, l) {}
    };
    struct ContinueStmt : Stmt {
        ContinueStmt(SourceLocation l) : Stmt(StmtKind::Continue, l) {}
    };

    // ===========================================================================
    // Printing / naming
    // ===========================================================================

    const char* binOpName(BinOp op);
    const char* unOpName(UnOp op);

    void printExpr(const Expr* e, int depth = 0);
    void printBlock(const Block& b, int depth = 0);
    void printStmt(const Stmt* s, int depth = 0);

} // namespace nova