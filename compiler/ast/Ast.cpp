#include "Ast.hpp"
#include <cstdio>

namespace nova {

    // ---------- operator names ----------

    const char* binOpName(BinOp op) {
        switch (op) {
        case BinOp::Add:      return "+";
        case BinOp::Sub:      return "-";
        case BinOp::Mul:      return "*";
        case BinOp::Div:      return "/";
        case BinOp::FloorDiv: return "//";
        case BinOp::Mod:      return "%";
        case BinOp::Pow:      return "**";
        case BinOp::Eq:       return "==";
        case BinOp::NotEq:    return "!=";
        case BinOp::Lt:       return "<";
        case BinOp::Gt:       return ">";
        case BinOp::LtEq:     return "<=";
        case BinOp::GtEq:     return ">=";
        case BinOp::And:      return "and";
        case BinOp::Or:       return "or";
        case BinOp::In:       return "in";
        case BinOp::Is:       return "is";
        }
        return "?";
    }

    const char* unOpName(UnOp op) {
        switch (op) {
        case UnOp::Neg: return "-";
        case UnOp::Pos: return "+";
        case UnOp::Not: return "not";
        }
        return "?";
    }

    // ---------- helpers ----------

    static void indent(int n) {
        for (int i = 0; i < n; ++i) std::fputs("  ", stdout);
    }

    static void printTypeOrNull(const Expr* e) {
        if (!e) { std::printf("?"); return; }
        // Only NameRef and simple nesting printed inline for compactness.
        switch (e->kind) {
        case ExprKind::NameRef:
            std::printf("%s", static_cast<const NameRefExpr*>(e)->name.c_str());
            return;
        default:
            std::printf("<type-expr>");
            return;
        }
    }

    // ---------- expressions ----------

    void printExpr(const Expr* e, int depth) {
        if (!e) return;
        indent(depth);
        switch (e->kind) {
        case ExprKind::IntLit: {
            auto* n = static_cast<const IntLitExpr*>(e);
            std::printf("Int(%s)\n", n->text.c_str()); break;
        }
        case ExprKind::FloatLit: {
            auto* n = static_cast<const FloatLitExpr*>(e);
            std::printf("Float(%s)\n", n->text.c_str()); break;
        }
        case ExprKind::StringLit: {
            auto* n = static_cast<const StringLitExpr*>(e);
            std::printf("String(\"%s\")\n", n->value.c_str()); break;
        }
        case ExprKind::CharLit: {
            auto* n = static_cast<const CharLitExpr*>(e);
            std::printf("Char('%s')\n", n->value.c_str()); break;
        }
        case ExprKind::BoolLit: {
            auto* n = static_cast<const BoolLitExpr*>(e);
            std::printf("Bool(%s)\n", n->value ? "true" : "false"); break;
        }
        case ExprKind::NoneLit:
            std::printf("None\n"); break;
        case ExprKind::NameRef: {
            auto* n = static_cast<const NameRefExpr*>(e);
            std::printf("Name(%s)\n", n->name.c_str()); break;
        }
        case ExprKind::Unary: {
            auto* n = static_cast<const UnaryExpr*>(e);
            std::printf("Unary(%s)\n", unOpName(n->op));
            printExpr(n->operand.get(), depth + 1);
            break;
        }
        case ExprKind::Binary: {
            auto* n = static_cast<const BinaryExpr*>(e);
            std::printf("Binary(%s)\n", binOpName(n->op));
            printExpr(n->lhs.get(), depth + 1);
            printExpr(n->rhs.get(), depth + 1);
            break;
        }
        case ExprKind::Grouping: {
            auto* n = static_cast<const GroupingExpr*>(e);
            std::printf("Group\n");
            printExpr(n->inner.get(), depth + 1);
            break;
        }
        case ExprKind::Call: {
            auto* n = static_cast<const CallExpr*>(e);
            std::printf("Call\n");
            indent(depth + 1); std::printf("callee:\n");
            printExpr(n->callee.get(), depth + 2);
            if (!n->args.empty()) {
                indent(depth + 1); std::printf("args:\n");
                for (auto& a : n->args) printExpr(a.get(), depth + 2);
            }
            break;
        }
        case ExprKind::Attr: {
            auto* n = static_cast<const AttrExpr*>(e);
            std::printf("Attr(.%s)\n", n->name.c_str());
            printExpr(n->target.get(), depth + 1);
            break;
        }
        case ExprKind::Index: {
            auto* n = static_cast<const IndexExpr*>(e);
            std::printf("Index\n");
            printExpr(n->target.get(), depth + 1);
            printExpr(n->index.get(), depth + 1);
            break;
        }
        }
    }

    // ---------- statements ----------

    void printBlock(const Block& b, int depth) {
        if (b.stmts.empty()) {
            indent(depth); std::printf("(empty)\n");
            return;
        }
        for (auto& s : b.stmts) printStmt(s.get(), depth);
    }

    void printStmt(const Stmt* s, int depth) {
        if (!s) return;
        switch (s->kind) {
        case StmtKind::Expr: {
            auto* n = static_cast<const ExprStmt*>(s);
            indent(depth); std::printf("ExprStmt\n");
            printExpr(n->expr.get(), depth + 1);
            break;
        }
        case StmtKind::Assign: {
            auto* n = static_cast<const AssignStmt*>(s);
            indent(depth); std::printf("Assign\n");
            indent(depth + 1); std::printf("target:\n");
            printExpr(n->target.get(), depth + 2);
            indent(depth + 1); std::printf("value:\n");
            printExpr(n->value.get(), depth + 2);
            break;
        }
        case StmtKind::AnnotAssign: {
            auto* n = static_cast<const AnnotAssignStmt*>(s);
            indent(depth); std::printf("AnnotAssign(%s: ", n->name.c_str());
            printTypeOrNull(n->type.get());
            std::printf(")\n");
            if (n->value) {
                indent(depth + 1); std::printf("value:\n");
                printExpr(n->value.get(), depth + 2);
            }
            break;
        }
        case StmtKind::If: {
            auto* n = static_cast<const IfStmt*>(s);
            indent(depth); std::printf("If\n");
            indent(depth + 1); std::printf("cond:\n");
            printExpr(n->cond.get(), depth + 2);
            indent(depth + 1); std::printf("then:\n");
            printBlock(n->thenBody, depth + 2);
            for (auto& e : n->elifs) {
                indent(depth + 1); std::printf("elif:\n");
                indent(depth + 2); std::printf("cond:\n");
                printExpr(e.cond.get(), depth + 3);
                indent(depth + 2); std::printf("then:\n");
                printBlock(e.body, depth + 3);
            }
            if (n->elseBody) {
                indent(depth + 1); std::printf("else:\n");
                printBlock(*n->elseBody, depth + 2);
            }
            break;
        }
        case StmtKind::While: {
            auto* n = static_cast<const WhileStmt*>(s);
            indent(depth); std::printf("While\n");
            indent(depth + 1); std::printf("cond:\n");
            printExpr(n->cond.get(), depth + 2);
            indent(depth + 1); std::printf("body:\n");
            printBlock(n->body, depth + 2);
            break;
        }
        case StmtKind::Def: {
            auto* n = static_cast<const DefStmt*>(s);
            indent(depth); std::printf("Def(%s)\n", n->name.c_str());
            indent(depth + 1); std::printf("params:\n");
            if (n->params.empty()) {
                indent(depth + 2); std::printf("(none)\n");
            }
            else {
                for (auto& p : n->params) {
                    indent(depth + 2); std::printf("%s: ", p.name.c_str());
                    printTypeOrNull(p.type.get());
                    std::printf("\n");
                }
            }
            if (n->returnType) {
                indent(depth + 1); std::printf("return: ");
                printTypeOrNull(n->returnType.get());
                std::printf("\n");
            }
            indent(depth + 1); std::printf("body:\n");
            printBlock(n->body, depth + 2);
            break;
        }
        case StmtKind::Return: {
            auto* n = static_cast<const ReturnStmt*>(s);
            indent(depth); std::printf("Return\n");
            if (n->value) printExpr(n->value.get(), depth + 1);
            break;
        }
        case StmtKind::Pass: {
            indent(depth); std::printf("Pass\n"); break;
        }
        case StmtKind::Break: {
            indent(depth); std::printf("Break\n"); break;
        }
        case StmtKind::Continue: {
            indent(depth); std::printf("Continue\n"); break;
        }
        }
    }

} // namespace nova