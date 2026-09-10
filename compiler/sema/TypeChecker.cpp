#include "TypeChecker.hpp"

namespace nova {

    // ===========================================================================
    // Entry
    // ===========================================================================

    void TypeChecker::check(const Block& program) {
        pushScope();
        installBuiltins();
        collectSignatures(program);

        for (auto& s : program.stmts) checkStmt(s.get());

        popScope();
    }

    // ===========================================================================
    // Scopes
    // ===========================================================================

    void TypeChecker::pushScope() { scopes_.emplace_back(); }
    void TypeChecker::popScope() { scopes_.pop_back(); }

    void TypeChecker::defineVar(const std::string& name, TypePtr t) {
        scopes_.back().vars[name] = std::move(t);
    }

    TypePtr TypeChecker::lookupVar(const std::string& name) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto f = it->vars.find(name);
            if (f != it->vars.end()) return f->second;
        }
        return nullptr;
    }

    // ===========================================================================
    // Errors
    // ===========================================================================

    [[noreturn]] void TypeChecker::error(SourceLocation loc, const std::string& msg) {
        throw TypeError(msg, loc);
    }

    // ===========================================================================
    // Builtins
    // ===========================================================================

    void TypeChecker::installBuiltins() {
        auto A = Types::Any();
        defineVar("print", Types::Function({ A }, Types::None()));
        defineVar("str", Types::Function({ A }, Types::Str()));
        defineVar("int", Types::Function({ A }, Types::Int()));
        defineVar("float", Types::Function({ A }, Types::Float()));
        defineVar("bool", Types::Function({ A }, Types::Bool()));
        defineVar("len", Types::Function({ A }, Types::Int()));
        defineVar("type", Types::Function({ A }, Types::Str()));
        defineVar("abs", Types::Function({ A }, A));
        defineVar("min", Types::Function({ A, A }, A));
        defineVar("max", Types::Function({ A, A }, A));
    }

    // ===========================================================================
    // Pass 1 — signatures
    // ===========================================================================

    void TypeChecker::collectSignatures(const Block& program) {
        for (auto& s : program.stmts) {
            if (s->kind != StmtKind::Def) continue;
            auto* d = static_cast<const DefStmt*>(s.get());

            std::vector<TypePtr> params;
            for (auto& p : d->params) {
                params.push_back(p.type ? resolveTypeExpr(p.type.get()) : Types::Any());
            }
            TypePtr ret = d->returnType ? resolveTypeExpr(d->returnType.get())
                : Types::None();

            if (functions_.count(d->name)) {
                error(d->loc, "function '" + d->name + "' already defined");
            }
            auto sig = Types::Function(std::move(params), std::move(ret));
            functions_[d->name] = sig;
            defineVar(d->name, sig);
        }
    }

    // ===========================================================================
    // Type expressions
    // ===========================================================================

    TypePtr TypeChecker::resolveTypeExpr(const Expr* e) {
        if (!e) return Types::Any();
        if (e->kind != ExprKind::NameRef) {
            error(e->loc, "expected a type name");
        }
        const auto* n = static_cast<const NameRefExpr*>(e);
        const std::string& name = n->name;

        if (name == "int")   return Types::Int();
        if (name == "float") return Types::Float();
        if (name == "bool")  return Types::Bool();
        if (name == "str")   return Types::Str();
        if (name == "char")  return Types::Char();
        if (name == "bytes") return Types::Bytes();
        if (name == "None")  return Types::None();
        if (name == "any")   return Types::Any();

        auto t = std::make_shared<Type>(TypeKind::Named);
        t->name = name;
        return t;
    }

    // ===========================================================================
    // Statements
    // ===========================================================================

    void TypeChecker::checkBlock(const Block& b) {
        pushScope();
        for (auto& s : b.stmts) checkStmt(s.get());
        popScope();
    }

    void TypeChecker::checkStmt(const Stmt* s) {
        if (!s) return;

        switch (s->kind) {

        case StmtKind::Expr: {
            auto* n = static_cast<const ExprStmt*>(s);
            checkExpr(n->expr.get());
            return;
        }

        case StmtKind::Assign: {
            auto* n = static_cast<const AssignStmt*>(s);
            TypePtr v = checkExpr(n->value.get());

            if (n->target->kind != ExprKind::NameRef) {
                error(n->target->loc, "assignment target must be a name");
            }
            const auto* nm = static_cast<const NameRefExpr*>(n->target.get());

            TypePtr existing = lookupVar(nm->name);
            if (existing) {
                if (!isAssignable(existing, v)) {
                    error(n->loc,
                        "cannot assign " + v->toString() + " to '" + nm->name +
                        "' of type " + existing->toString());
                }
            }
            else {
                defineVar(nm->name, v);
            }
            return;
        }

        case StmtKind::AnnotAssign: {
            auto* n = static_cast<const AnnotAssignStmt*>(s);
            TypePtr declared = resolveTypeExpr(n->type.get());
            if (n->value) {
                TypePtr v = checkExpr(n->value.get());
                if (!isAssignable(declared, v)) {
                    error(n->loc,
                        "cannot initialize '" + n->name + "' (" +
                        declared->toString() + ") with value of type " +
                        v->toString());
                }
            }
            defineVar(n->name, declared);
            return;
        }

        case StmtKind::If: {
            auto* n = static_cast<const IfStmt*>(s);
            checkExpr(n->cond.get());
            checkBlock(n->thenBody);
            for (auto& ec : n->elifs) {
                checkExpr(ec.cond.get());
                checkBlock(ec.body);
            }
            if (n->elseBody) checkBlock(*n->elseBody);
            return;
        }

        case StmtKind::While: {
            auto* n = static_cast<const WhileStmt*>(s);
            checkExpr(n->cond.get());
            ++loopDepth_;
            checkBlock(n->body);
            --loopDepth_;
            return;
        }

        case StmtKind::Def: {
            auto* n = static_cast<const DefStmt*>(s);
            auto it = functions_.find(n->name);
            if (it == functions_.end()) {
                error(n->loc, "internal: missing signature for '" + n->name + "'");
            }
            TypePtr sig = it->second;

            pushScope();
            for (size_t i = 0; i < n->params.size(); ++i) {
                defineVar(n->params[i].name, sig->params[i]);
            }

            TypePtr savedRet = currentReturnType_;
            int     savedLoop = loopDepth_;
            currentReturnType_ = sig->returnType;
            loopDepth_ = 0;

            for (auto& st : n->body.stmts) checkStmt(st.get());

            currentReturnType_ = savedRet;
            loopDepth_ = savedLoop;
            popScope();
            return;
        }

        case StmtKind::Return: {
            auto* n = static_cast<const ReturnStmt*>(s);
            if (!currentReturnType_) {
                error(n->loc, "'return' outside function");
            }
            TypePtr v = n->value ? checkExpr(n->value.get()) : Types::None();
            if (!isAssignable(currentReturnType_, v)) {
                error(n->loc,
                    "returning " + v->toString() +
                    " from function declared to return " +
                    currentReturnType_->toString());
            }
            return;
        }

        case StmtKind::Pass: return;

        case StmtKind::Break:
        case StmtKind::Continue:
            if (loopDepth_ == 0) {
                error(s->loc, s->kind == StmtKind::Break
                    ? "'break' outside loop"
                    : "'continue' outside loop");
            }
            return;
        }
    }

    // ===========================================================================
    // Expressions
    // ===========================================================================

    TypePtr TypeChecker::checkExpr(const Expr* e) {
        if (!e) return Types::None();

        switch (e->kind) {
        case ExprKind::IntLit:    return Types::Int();
        case ExprKind::FloatLit:  return Types::Float();
        case ExprKind::StringLit: return Types::Str();
        case ExprKind::CharLit:   return Types::Char();
        case ExprKind::BoolLit:   return Types::Bool();
        case ExprKind::NoneLit:   return Types::None();

        case ExprKind::NameRef: {
            auto* n = static_cast<const NameRefExpr*>(e);
            TypePtr t = lookupVar(n->name);
            if (!t) error(n->loc, "name '" + n->name + "' is not defined");
            return t;
        }

        case ExprKind::Grouping:
            return checkExpr(static_cast<const GroupingExpr*>(e)->inner.get());

        case ExprKind::Unary: {
            auto* n = static_cast<const UnaryExpr*>(e);
            TypePtr t = checkExpr(n->operand.get());
            switch (n->op) {
            case UnOp::Not: return Types::Bool();
            case UnOp::Neg:
            case UnOp::Pos:
                if (t->kind == TypeKind::Int)    return Types::Int();
                if (t->kind == TypeKind::Float)  return Types::Float();
                if (t->kind == TypeKind::Any)    return Types::Any();
                if (t->kind == TypeKind::Error)  return t;
                error(n->loc, "cannot apply unary operator to " + t->toString());
            }
            return Types::Error();
        }

        case ExprKind::Binary: {
            auto* n = static_cast<const BinaryExpr*>(e);

            if (n->op == BinOp::And || n->op == BinOp::Or) {
                checkExpr(n->lhs.get());
                checkExpr(n->rhs.get());
                return Types::Bool();
            }

            TypePtr lt = checkExpr(n->lhs.get());
            TypePtr rt = checkExpr(n->rhs.get());

            if (lt->kind == TypeKind::Error || rt->kind == TypeKind::Error)
                return Types::Error();
            if (lt->kind == TypeKind::Any || rt->kind == TypeKind::Any) {
                // For comparison ops, always bool.
                switch (n->op) {
                case BinOp::Eq: case BinOp::NotEq:
                case BinOp::Lt: case BinOp::Gt:
                case BinOp::LtEq: case BinOp::GtEq:
                case BinOp::In: case BinOp::Is:
                    return Types::Bool();
                default: return Types::Any();
                }
            }

            switch (n->op) {
            case BinOp::Add:
                if (lt->kind == TypeKind::Int && rt->kind == TypeKind::Int)   return Types::Int();
                if (lt->kind == TypeKind::Float && rt->kind == TypeKind::Float) return Types::Float();
                if (lt->kind == TypeKind::Int && rt->kind == TypeKind::Float) return Types::Float();
                if (lt->kind == TypeKind::Float && rt->kind == TypeKind::Int)   return Types::Float();
                if (lt->kind == TypeKind::Str && rt->kind == TypeKind::Str)   return Types::Str();
                error(n->loc, "cannot add " + lt->toString() +
                    " and " + rt->toString());

            case BinOp::Sub: {
                TypePtr c = commonNumeric(lt, rt);
                if (c->kind == TypeKind::Error)
                    error(n->loc, "cannot subtract " + rt->toString() +
                        " from " + lt->toString());
                return c;
            }

            case BinOp::Mul:
                if (lt->kind == TypeKind::Str && rt->kind == TypeKind::Int) return Types::Str();
                if (lt->kind == TypeKind::Int && rt->kind == TypeKind::Str) return Types::Str();
                {
                    TypePtr c = commonNumeric(lt, rt);
                    if (c->kind == TypeKind::Error)
                        error(n->loc, "cannot multiply " + lt->toString() +
                            " by " + rt->toString());
                    return c;
                }

            case BinOp::Div: {
                bool ln = lt->kind == TypeKind::Int || lt->kind == TypeKind::Float;
                bool rn = rt->kind == TypeKind::Int || rt->kind == TypeKind::Float;
                if (!ln || !rn)
                    error(n->loc, "cannot divide " + lt->toString() +
                        " by " + rt->toString());
                return Types::Float();
            }

            case BinOp::FloorDiv:
            case BinOp::Mod:
            case BinOp::Pow: {
                TypePtr c = commonNumeric(lt, rt);
                if (c->kind == TypeKind::Error)
                    error(n->loc, std::string("cannot apply '") +
                        binOpName(n->op) + "' to " +
                        lt->toString() + " and " + rt->toString());
                return c;
            }

            case BinOp::Eq: case BinOp::NotEq:
            case BinOp::Lt: case BinOp::Gt:
            case BinOp::LtEq: case BinOp::GtEq:
            case BinOp::In: case BinOp::Is:
                return Types::Bool();

            default:
                return Types::Error();
            }
        }

        case ExprKind::Call: {
            auto* n = static_cast<const CallExpr*>(e);
            TypePtr callee = checkExpr(n->callee.get());

            std::vector<TypePtr> argTypes;
            argTypes.reserve(n->args.size());
            for (auto& a : n->args) argTypes.push_back(checkExpr(a.get()));

            if (callee->kind == TypeKind::Error) return Types::Error();
            if (callee->kind == TypeKind::Any)   return Types::Any();

            if (callee->kind != TypeKind::Function) {
                error(n->loc, "cannot call value of type " + callee->toString());
            }

            // Variadic builtins: skip arity check.
            const NameRefExpr* nm =
                (n->callee->kind == ExprKind::NameRef)
                ? static_cast<const NameRefExpr*>(n->callee.get())
                : nullptr;
            if (nm && (nm->name == "print" || nm->name == "min" || nm->name == "max")) {
                return callee->returnType ? callee->returnType : Types::None();
            }

            if (argTypes.size() != callee->params.size()) {
                error(n->loc,
                    "function expects " + std::to_string(callee->params.size()) +
                    " argument(s), got " + std::to_string(argTypes.size()));
            }
            for (size_t i = 0; i < argTypes.size(); ++i) {
                if (!isAssignable(callee->params[i], argTypes[i])) {
                    error(n->args[i]->loc,
                        "argument " + std::to_string(i + 1) + ": expected " +
                        callee->params[i]->toString() + ", got " +
                        argTypes[i]->toString());
                }
            }
            return callee->returnType ? callee->returnType : Types::None();
        }

        case ExprKind::Attr:
        case ExprKind::Index:
            error(e->loc, "not yet supported by the type checker");
        }
        return Types::Error();
    }

} // namespace nova