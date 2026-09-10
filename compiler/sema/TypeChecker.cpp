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
    // Pass 1 — collect structs and function signatures
    // ===========================================================================

    void TypeChecker::collectSignatures(const Block& program) {
        // Structs first — function signatures may reference them.
        for (auto& s : program.stmts) {
            if (s->kind != StmtKind::Struct) continue;
            auto* d = static_cast<const StructStmt*>(s.get());
            if (structs_.count(d->name)) error(d->loc, "struct '" + d->name + "' already defined");

            // Create a placeholder struct type so it can be referenced recursively
            auto st = Types::Struct(d->name, {});
            structs_[d->name] = st;
            structDecls_[d->name] = d;

            // Now fill in fields (must resolve to already-known types)
            std::vector<StructFieldInfo> fields;
            for (auto& f : d->fields) {
                for (auto& existing : fields)
                    if (existing.name == f.name)
                        error(f.loc, "duplicate field '" + f.name + "' in struct '" + d->name + "'");
                TypePtr ft = resolveTypeExpr(f.type.get());
                fields.push_back({ f.name, ft });
            }
            st->fields = std::move(fields);
        }

        // Functions
        for (auto& s : program.stmts) {
            if (s->kind != StmtKind::Def) continue;
            auto* d = static_cast<const DefStmt*>(s.get());

            std::vector<TypePtr> params;
            for (auto& p : d->params)
                params.push_back(p.type ? resolveTypeExpr(p.type.get()) : Types::Any());

            TypePtr ret = d->returnType ? resolveTypeExpr(d->returnType.get())
                : Types::None();

            if (functions_.count(d->name))
                error(d->loc, "function '" + d->name + "' already defined");

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
        if (e->kind != ExprKind::NameRef)
            error(e->loc, "expected a type name");

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

        auto it = structs_.find(name);
        if (it != structs_.end()) return it->second;

        // Unknown name — treat as named type (no generic types yet).
        auto t = std::make_shared<Type>(TypeKind::Named);
        t->name = name;
        return t;
    }

    // ===========================================================================
    // Struct construction
    // ===========================================================================

    TypePtr TypeChecker::checkStructConstruction(const StructStmt* decl,
        const CallExpr* call,
        const std::string& structName) {
        TypePtr st = structs_.at(structName);

        std::vector<bool> seen(decl->fields.size(), false);
        size_t positional = 0;

        for (const auto& arg : call->args) {
            TypePtr at = checkExpr(arg.value.get());

            if (arg.name.empty()) {
                if (positional >= decl->fields.size())
                    error(arg.loc, "too many positional arguments for struct '" +
                        structName + "'");
                const auto& f = decl->fields[positional];
                TypePtr ft = st->fields[positional].type;
                if (!isAssignable(ft, at))
                    error(arg.loc, "field '" + f.name + "' expects " +
                        ft->toString() + ", got " + at->toString());
                seen[positional] = true;
                ++positional;
            }
            else {
                // keyword — find matching field
                int idx = -1;
                for (size_t i = 0; i < decl->fields.size(); ++i)
                    if (decl->fields[i].name == arg.name) { idx = (int)i; break; }
                if (idx < 0)
                    error(arg.loc, "struct '" + structName + "' has no field '" +
                        arg.name + "'");
                if (seen[idx])
                    error(arg.loc, "field '" + arg.name + "' given more than once");
                TypePtr ft = st->fields[idx].type;
                if (!isAssignable(ft, at))
                    error(arg.loc, "field '" + arg.name + "' expects " +
                        ft->toString() + ", got " + at->toString());
                seen[idx] = true;
            }
        }

        // Every field must be initialized.
        for (size_t i = 0; i < decl->fields.size(); ++i) {
            if (!seen[i])
                error(call->loc, "struct '" + structName + "' is missing value for field '" +
                    decl->fields[i].name + "'");
        }
        return st;
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

            // -------- Name target --------
            if (n->target->kind == ExprKind::NameRef) {
                TypePtr v = checkExpr(n->value.get());
                const auto* nm = static_cast<const NameRefExpr*>(n->target.get());
                TypePtr existing = lookupVar(nm->name);
                if (existing) {
                    if (!isAssignable(existing, v))
                        error(n->loc, "cannot assign " + v->toString() +
                            " to '" + nm->name + "' of type " +
                            existing->toString());
                }
                else {
                    defineVar(nm->name, v);
                }
                return;
            }

            // -------- Attr target: instance.field = value --------
            if (n->target->kind == ExprKind::Attr) {
                auto* a = static_cast<const AttrExpr*>(n->target.get());
                TypePtr tTarget = checkExpr(a->target.get());

                if (tTarget->kind != TypeKind::Struct) {
                    error(a->loc, "cannot set field '" + a->name +
                        "' on value of type " + tTarget->toString());
                }
                const StructFieldInfo* f = tTarget->findField(a->name);
                if (!f)
                    error(a->loc, "struct '" + tTarget->name +
                        "' has no field '" + a->name + "'");

                TypePtr v = checkExpr(n->value.get());
                if (!isAssignable(f->type, v))
                    error(n->loc, "field '" + a->name + "' expects " +
                        f->type->toString() + ", got " + v->toString());
                return;
            }

            error(n->target->loc, "invalid assignment target");
        }

        case StmtKind::AnnotAssign: {
            auto* n = static_cast<const AnnotAssignStmt*>(s);
            TypePtr declared = resolveTypeExpr(n->type.get());
            if (n->value) {
                TypePtr v = checkExpr(n->value.get());
                if (!isAssignable(declared, v))
                    error(n->loc, "cannot initialize '" + n->name + "' (" +
                        declared->toString() + ") with value of type " +
                        v->toString());
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
            if (it == functions_.end())
                error(n->loc, "internal: missing signature for '" + n->name + "'");
            TypePtr sig = it->second;

            pushScope();
            for (size_t i = 0; i < n->params.size(); ++i)
                defineVar(n->params[i].name, sig->params[i]);

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
            if (!currentReturnType_) error(n->loc, "'return' outside function");
            TypePtr v = n->value ? checkExpr(n->value.get()) : Types::None();
            if (!isAssignable(currentReturnType_, v))
                error(n->loc, "returning " + v->toString() +
                    " from function declared to return " +
                    currentReturnType_->toString());
            return;
        }

        case StmtKind::Struct:
            // Already fully processed in pass 1.
            return;

        case StmtKind::Pass: return;

        case StmtKind::Break:
        case StmtKind::Continue:
            if (loopDepth_ == 0)
                error(s->loc, s->kind == StmtKind::Break
                    ? "'break' outside loop"
                    : "'continue' outside loop");
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
            if (t) return t;

            // Not a variable — could be a struct used as a constructor.
            auto it = structs_.find(n->name);
            if (it != structs_.end()) return it->second;

            error(n->loc, "name '" + n->name + "' is not defined");
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
                if (t->kind == TypeKind::Int)   return Types::Int();
                if (t->kind == TypeKind::Float) return Types::Float();
                if (t->kind == TypeKind::Any)   return Types::Any();
                if (t->kind == TypeKind::Error) return t;
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
            default: return Types::Error();
            }
        }

        case ExprKind::Attr: {
            auto* n = static_cast<const AttrExpr*>(e);
            TypePtr t = checkExpr(n->target.get());
            if (t->kind == TypeKind::Error) return t;
            if (t->kind == TypeKind::Any)   return Types::Any();
            if (t->kind != TypeKind::Struct)
                error(n->loc, "cannot read field '" + n->name +
                    "' on value of type " + t->toString());
            const StructFieldInfo* f = t->findField(n->name);
            if (!f)
                error(n->loc, "struct '" + t->name +
                    "' has no field '" + n->name + "'");
            return f->type;
        }

        case ExprKind::Call: {
            auto* n = static_cast<const CallExpr*>(e);

            // Struct construction?
            if (n->callee->kind == ExprKind::NameRef) {
                const auto* nm = static_cast<const NameRefExpr*>(n->callee.get());
                auto it = structDecls_.find(nm->name);
                if (it != structDecls_.end()) {
                    return checkStructConstruction(it->second, n, nm->name);
                }
            }

            TypePtr callee = checkExpr(n->callee.get());

            std::vector<TypePtr> argTypes;
            for (auto& a : n->args) argTypes.push_back(checkExpr(a.value.get()));

            if (callee->kind == TypeKind::Error) return Types::Error();
            if (callee->kind == TypeKind::Any)   return Types::Any();
            if (callee->kind != TypeKind::Function)
                error(n->loc, "cannot call value of type " + callee->toString());

            const NameRefExpr* nm =
                (n->callee->kind == ExprKind::NameRef)
                ? static_cast<const NameRefExpr*>(n->callee.get())
                : nullptr;
            if (nm && (nm->name == "print" || nm->name == "min" || nm->name == "max"))
                return callee->returnType ? callee->returnType : Types::None();

            if (argTypes.size() != callee->params.size())
                error(n->loc, "function expects " +
                    std::to_string(callee->params.size()) +
                    " argument(s), got " +
                    std::to_string(argTypes.size()));
            for (size_t i = 0; i < argTypes.size(); ++i)
                if (!isAssignable(callee->params[i], argTypes[i]))
                    error(n->args[i].loc,
                        "argument " + std::to_string(i + 1) + ": expected " +
                        callee->params[i]->toString() + ", got " +
                        argTypes[i]->toString());
            return callee->returnType ? callee->returnType : Types::None();
        }

        case ExprKind::Index:
            error(e->loc, "indexing not yet supported by the type checker");
        }
        return Types::Error();
    }

} // namespace nova