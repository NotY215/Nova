#include "Ast.hpp"
#include <cstdio>
#include <string>

namespace nova {

    // ===========================================================================
    // Operator names
    // ===========================================================================

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

    // ===========================================================================
    // Tree-printing helpers
    // ===========================================================================

    namespace {

        // UTF-8 box drawing.  Set the console to CP_UTF8 (done in main.cpp) so these
        // render correctly in Windows Terminal / VS debug console / PowerShell.
        constexpr const char* TEE = "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 "; // ├──
        constexpr const char* ELBOW = "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80 "; // └──
        constexpr const char* PIPE = "\xE2\x94\x82   ";                       // │
        constexpr const char* BLANK = "    ";

        void putLabel(const std::string& prefix, bool isLast, const std::string& label) {
            std::printf("%s%s%s\n",
                prefix.c_str(),
                isLast ? ELBOW : TEE,
                label.c_str());
        }

        std::string childPrefix(const std::string& prefix, bool isLast) {
            return prefix + (isLast ? BLANK : PIPE);
        }

    } // namespace

    // Forward declarations (internal linkage)
    static void prExpr(const Expr* e, const std::string& prefix, bool isLast);
    static void prStmt(const Stmt* s, const std::string& prefix, bool isLast);
    static void prBlock(const Block& b, const std::string& prefix);

    // ===========================================================================
    // Expressions
    // ===========================================================================

    static void prExpr(const Expr* e, const std::string& prefix, bool isLast) {
        if (!e) return;

        switch (e->kind) {
        case ExprKind::IntLit: {
            auto* n = static_cast<const IntLitExpr*>(e);
            putLabel(prefix, isLast, "Int(" + n->text + ")");
            break;
        }
        case ExprKind::FloatLit: {
            auto* n = static_cast<const FloatLitExpr*>(e);
            putLabel(prefix, isLast, "Float(" + n->text + ")");
            break;
        }
        case ExprKind::StringLit: {
            auto* n = static_cast<const StringLitExpr*>(e);
            putLabel(prefix, isLast, "String(\"" + n->value + "\")");
            break;
        }
        case ExprKind::CharLit: {
            auto* n = static_cast<const CharLitExpr*>(e);
            putLabel(prefix, isLast, "Char('" + n->value + "')");
            break;
        }
        case ExprKind::BoolLit: {
            auto* n = static_cast<const BoolLitExpr*>(e);
            putLabel(prefix, isLast, n->value ? "Bool(true)" : "Bool(false)");
            break;
        }
        case ExprKind::NoneLit:
            putLabel(prefix, isLast, "None");
            break;
        case ExprKind::NameRef: {
            auto* n = static_cast<const NameRefExpr*>(e);
            putLabel(prefix, isLast, "Name(" + n->name + ")");
            break;
        }
        case ExprKind::Unary: {
            auto* n = static_cast<const UnaryExpr*>(e);
            putLabel(prefix, isLast, std::string("Unary(") + unOpName(n->op) + ")");
            prExpr(n->operand.get(), childPrefix(prefix, isLast), true);
            break;
        }
        case ExprKind::Binary: {
            auto* n = static_cast<const BinaryExpr*>(e);
            putLabel(prefix, isLast, std::string("Binary(") + binOpName(n->op) + ")");
            std::string cp = childPrefix(prefix, isLast);
            prExpr(n->lhs.get(), cp, false);
            prExpr(n->rhs.get(), cp, true);
            break;
        }
        case ExprKind::Grouping: {
            auto* n = static_cast<const GroupingExpr*>(e);
            putLabel(prefix, isLast, "Group");
            prExpr(n->inner.get(), childPrefix(prefix, isLast), true);
            break;
        }
        case ExprKind::Call: {
            auto* n = static_cast<const CallExpr*>(e);
            putLabel(prefix, isLast, "Call");
            std::string cp = childPrefix(prefix, isLast);
            bool hasArgs = !n->args.empty();

            putLabel(cp, !hasArgs, "callee");
            prExpr(n->callee.get(), childPrefix(cp, !hasArgs), true);

            if (hasArgs) {
                putLabel(cp, true, "args");
                std::string ap = childPrefix(cp, true);
                for (size_t i = 0; i < n->args.size(); ++i) {
                    prExpr(n->args[i].get(), ap, i + 1 == n->args.size());
                }
            }
            break;
        }
        case ExprKind::Attr: {
            auto* n = static_cast<const AttrExpr*>(e);
            putLabel(prefix, isLast, "Attr(." + n->name + ")");
            prExpr(n->target.get(), childPrefix(prefix, isLast), true);
            break;
        }
        case ExprKind::Index: {
            auto* n = static_cast<const IndexExpr*>(e);
            putLabel(prefix, isLast, "Index");
            std::string cp = childPrefix(prefix, isLast);
            prExpr(n->target.get(), cp, false);
            prExpr(n->index.get(), cp, true);
            break;
        }
        }
    }

    // ===========================================================================
    // Statements
    // ===========================================================================

    static void prBlock(const Block& b, const std::string& prefix) {
        if (b.stmts.empty()) return;
        for (size_t i = 0; i < b.stmts.size(); ++i) {
            prStmt(b.stmts[i].get(), prefix, i + 1 == b.stmts.size());
        }
    }

    static void prStmt(const Stmt* s, const std::string& prefix, bool isLast) {
        if (!s) return;

        switch (s->kind) {
        case StmtKind::Expr: {
            auto* n = static_cast<const ExprStmt*>(s);
            putLabel(prefix, isLast, "ExprStmt");
            prExpr(n->expr.get(), childPrefix(prefix, isLast), true);
            break;
        }
        case StmtKind::Assign: {
            auto* n = static_cast<const AssignStmt*>(s);
            putLabel(prefix, isLast, "Assign");
            std::string cp = childPrefix(prefix, isLast);
            putLabel(cp, false, "target");
            prExpr(n->target.get(), childPrefix(cp, false), true);
            putLabel(cp, true, "value");
            prExpr(n->value.get(), childPrefix(cp, true), true);
            break;
        }
        case StmtKind::AnnotAssign: {
            auto* n = static_cast<const AnnotAssignStmt*>(s);
            std::string label = "AnnotAssign(" + n->name + ": ";
            // Type is a bare Name for simple cases.
            if (n->type && n->type->kind == ExprKind::NameRef) {
                label += static_cast<const NameRefExpr*>(n->type.get())->name;
            }
            else {
                label += "<type>";
            }
            label += ")";
            putLabel(prefix, isLast, label);
            if (n->value) {
                putLabel(childPrefix(prefix, isLast), true, "value");
                prExpr(n->value.get(), childPrefix(childPrefix(prefix, isLast), true), true);
            }
            break;
        }
        case StmtKind::If: {
            auto* n = static_cast<const IfStmt*>(s);
            putLabel(prefix, isLast, "If");
            std::string cp = childPrefix(prefix, isLast);

            size_t total = 2 + n->elifs.size() + (n->elseBody ? 1 : 0);
            size_t idx = 0;

            // cond
            putLabel(cp, false, "cond");
            prExpr(n->cond.get(), childPrefix(cp, false), true);
            ++idx;

            // then
            bool thenLast = (idx + 1 == total);
            putLabel(cp, thenLast, "then");
            prBlock(n->thenBody, childPrefix(cp, thenLast));
            ++idx;

            // elifs
            for (auto& ec : n->elifs) {
                bool eLast = (idx + 1 == total);
                putLabel(cp, eLast, "elif");
                std::string ep = childPrefix(cp, eLast);
                putLabel(ep, false, "cond");
                prExpr(ec.cond.get(), childPrefix(ep, false), true);
                putLabel(ep, true, "then");
                prBlock(ec.body, childPrefix(ep, true));
                ++idx;
            }

            // else
            if (n->elseBody) {
                putLabel(cp, true, "else");
                prBlock(*n->elseBody, childPrefix(cp, true));
            }
            break;
        }
        case StmtKind::While: {
            auto* n = static_cast<const WhileStmt*>(s);
            putLabel(prefix, isLast, "While");
            std::string cp = childPrefix(prefix, isLast);
            putLabel(cp, false, "cond");
            prExpr(n->cond.get(), childPrefix(cp, false), true);
            putLabel(cp, true, "body");
            prBlock(n->body, childPrefix(cp, true));
            break;
        }
        case StmtKind::Def: {
            auto* n = static_cast<const DefStmt*>(s);
            std::string label = "Def " + n->name + "(";
            for (size_t i = 0; i < n->params.size(); ++i) {
                if (i) label += ", ";
                label += n->params[i].name;
                if (n->params[i].type &&
                    n->params[i].type->kind == ExprKind::NameRef) {
                    label += ": ";
                    label += static_cast<const NameRefExpr*>(
                        n->params[i].type.get())->name;
                }
            }
            label += ")";
            if (n->returnType && n->returnType->kind == ExprKind::NameRef) {
                label += " -> ";
                label += static_cast<const NameRefExpr*>(n->returnType.get())->name;
            }
            putLabel(prefix, isLast, label);
            if (!n->body.stmts.empty()) {
                prBlock(n->body, childPrefix(prefix, isLast));
            }
            break;
        }
        case StmtKind::Return: {
            auto* n = static_cast<const ReturnStmt*>(s);
            putLabel(prefix, isLast, "Return");
            if (n->value) prExpr(n->value.get(), childPrefix(prefix, isLast), true);
            break;
        }
        case StmtKind::Pass:
            putLabel(prefix, isLast, "Pass"); break;
        case StmtKind::Break:
            putLabel(prefix, isLast, "Break"); break;
        case StmtKind::Continue:
            putLabel(prefix, isLast, "Continue"); break;
        }
    }

    // ===========================================================================
    // Public entry point
    // ===========================================================================

    void printProgram(const Block& program) {
        std::printf("Program\n");
        prBlock(program, "");
    }

} // namespace nova