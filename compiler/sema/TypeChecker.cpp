#include "TypeChecker.hpp"

namespace nova {

    void TypeChecker::check(const Block& program) {
        pushScope();
        installBuiltins();
        collectSignatures(program);
        for (auto& s : program.stmts) checkStmt(s.get());
        popScope();
    }

    void TypeChecker::pushScope() { scopes_.emplace_back(); }
    void TypeChecker::popScope() { scopes_.pop_back(); }
    void TypeChecker::defineVar(const std::string& n, TypePtr t) { scopes_.back().vars[n] = std::move(t); }
    TypePtr TypeChecker::lookupVar(const std::string& n) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto f = it->vars.find(n);
            if (f != it->vars.end()) return f->second;
        }
        return nullptr;
    }
    [[noreturn]] void TypeChecker::error(SourceLocation loc, const std::string& msg) {
        throw TypeError(msg, loc);
    }

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
    // Pass 1 — struct / class / function signatures
    // ===========================================================================

    void TypeChecker::collectSignatures(const Block& program) {
        // --- structs ---
        for (auto& s : program.stmts) {
            if (s->kind != StmtKind::Struct) continue;
            auto* d = static_cast<const StructStmt*>(s.get());
            if (structs_.count(d->name)) error(d->loc, "struct '" + d->name + "' already defined");
            auto st = Types::Struct(d->name, {});
            structs_[d->name] = st;
            std::vector<StructFieldInfo> fields;
            for (auto& f : d->fields) {
                for (auto& e : fields) if (e.name == f.name)
                    error(f.loc, "duplicate field '" + f.name + "' in struct '" + d->name + "'");
                fields.push_back({ f.name, resolveTypeExpr(f.type.get()) });
            }
            st->fields = std::move(fields);
        }

        // --- classes ---
        for (int pass = 0; pass < 2; ++pass) {
            for (auto& s : program.stmts) {
                if (s->kind != StmtKind::Class) continue;
                auto* d = static_cast<const ClassStmt*>(s.get());

                if (pass == 0) {
                    if (structs_.count(d->name)) continue;
                    auto ct = Types::Struct(d->name, {});
                    structs_[d->name] = ct;
                    continue;
                }
                if (!structs_.count(d->name)) continue;
                auto ct = structs_[d->name];
                if (!d->parentName.empty()) {
                    auto it = structs_.find(d->parentName);
                    if (it == structs_.end())
                        error(d->loc, "unknown parent class '" + d->parentName + "'");
                    if (it->second->kind != TypeKind::Struct)
                        error(d->loc, "'" + d->parentName + "' is not a class");
                    if (it->second->name == d->name)
                        error(d->loc, "class '" + d->name + "' cannot inherit from itself");
                    ct->parent = it->second;
                }
                std::vector<StructFieldInfo> fields;
                for (auto& f : d->fields) {
                    for (auto& e : fields) if (e.name == f.name)
                        error(f.loc, "duplicate field '" + f.name + "' in class '" + d->name + "'");
                    if (ct->findField(f.name))
                        error(f.loc, "field '" + f.name + "' re-declared in subclass");
                    fields.push_back({ f.name, resolveTypeExpr(f.type.get()) });
                }
                ct->fields = std::move(fields);
                for (auto& m : d->methods) {
                    if (ct->methods.count(m->name))
                        error(m->loc, "method '" + m->name + "' declared twice");
                    if (ct->findField(m->name))
                        error(m->loc, "field '" + m->name + "' already exists; cannot also be a method");
                    std::vector<TypePtr> params;
                    for (size_t i = 0; i < m->params.size(); ++i) {
                        if (i == 0) params.push_back(ct);
                        else params.push_back(m->params[i].type
                            ? resolveTypeExpr(m->params[i].type.get())
                            : Types::Any());
                    }
                    TypePtr ret = m->returnType ? resolveTypeExpr(m->returnType.get())
                        : Types::None();
                    ct->methods[m->name] = Types::Function(std::move(params), ret);
                }
            }
        }

        // --- functions ---
        for (auto& s : program.stmts) {
            if (s->kind != StmtKind::Def) continue;
            auto* d = static_cast<const DefStmt*>(s.get());
            std::vector<TypePtr> params;
            for (auto& p : d->params)
                params.push_back(p.type ? resolveTypeExpr(p.type.get()) : Types::Any());
            TypePtr ret = d->returnType ? resolveTypeExpr(d->returnType.get()) : Types::None();
            if (functions_.count(d->name)) error(d->loc, "function '" + d->name + "' already defined");
            auto sig = Types::Function(std::move(params), std::move(ret));
            functions_[d->name] = sig;
            defineVar(d->name, sig);
        }
    }

    // ===========================================================================
    // Type resolution
    // ===========================================================================

    TypePtr TypeChecker::resolveTypeExpr(const Expr* e) {
        if (!e) return Types::Any();
        if (e->kind != ExprKind::NameRef) error(e->loc, "expected a type name");
        const std::string& n = static_cast<const NameRefExpr*>(e)->name;
        if (n == "int")   return Types::Int();
        if (n == "float") return Types::Float();
        if (n == "bool")  return Types::Bool();
        if (n == "str")   return Types::Str();
        if (n == "char")  return Types::Char();
        if (n == "bytes") return Types::Bytes();
        if (n == "None")  return Types::None();
        if (n == "any")   return Types::Any();
        auto it = structs_.find(n);
        if (it != structs_.end()) return it->second;
        auto t = std::make_shared<Type>(TypeKind::Named); t->name = n; return t;
    }

    // ===========================================================================
    // Struct construction (also used by classes without __init__)
    // ===========================================================================

    TypePtr TypeChecker::checkStructConstruction(const StructStmt* decl,
        const CallExpr* call,
        const std::string& name) {
        TypePtr st = structs_.at(name);
        std::vector<bool> seen(decl->fields.size(), false);
        size_t positional = 0;
        for (const auto& arg : call->args) {
            TypePtr at = checkExpr(arg.value.get());
            if (arg.name.empty()) {
                if (positional >= decl->fields.size())
                    error(arg.loc, "too many positional arguments for struct '" + name + "'");
                TypePtr ft = st->fields[positional].type;
                if (!isAssignable(ft, at))
                    error(arg.loc, "field '" + decl->fields[positional].name +
                        "' expects " + ft->toString() + ", got " + at->toString());
                seen[positional] = true; ++positional;
            }
            else {
                int idx = -1;
                for (size_t i = 0; i < decl->fields.size(); ++i)
                    if (decl->fields[i].name == arg.name) { idx = (int)i; break; }
                if (idx < 0) error(arg.loc, "struct '" + name + "' has no field '" + arg.name + "'");
                if (seen[idx]) error(arg.loc, "field '" + arg.name + "' given more than once");
                TypePtr ft = st->fields[idx].type;
                if (!isAssignable(ft, at))
                    error(arg.loc, "field '" + arg.name + "' expects " +
                        ft->toString() + ", got " + at->toString());
                seen[idx] = true;
            }
        }
        for (size_t i = 0; i < decl->fields.size(); ++i)
            if (!seen[i]) error(call->loc, "struct '" + name + "' is missing value for field '" +
                decl->fields[i].name + "'");
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

    void TypeChecker::checkMethodBody(const DefStmt* m, TypePtr cls) {
        auto sig = cls->methods.at(m->name);
        pushScope();
        for (size_t i = 0; i < m->params.size(); ++i)
            defineVar(m->params[i].name, i == 0 ? cls : sig->params[i]);

        TypePtr savedRet = currentReturnType_;
        TypePtr savedClass = currentClass_;
        int     savedLoop = loopDepth_;
        currentReturnType_ = sig->returnType;
        currentClass_ = cls;
        loopDepth_ = 0;

        for (auto& st : m->body.stmts) checkStmt(st.get());

        currentReturnType_ = savedRet;
        currentClass_ = savedClass;
        loopDepth_ = savedLoop;
        popScope();
    }

    void TypeChecker::checkStmt(const Stmt* s) {
        if (!s) return;
        switch (s->kind) {

        case StmtKind::Expr: {
            auto* n = static_cast<const ExprStmt*>(s);
            checkExpr(n->expr.get()); return;
        }

        case StmtKind::Assign: {
            auto* n = static_cast<const AssignStmt*>(s);
            if (n->target->kind == ExprKind::NameRef) {
                TypePtr v = checkExpr(n->value.get());
                const auto* nm = static_cast<const NameRefExpr*>(n->target.get());
                TypePtr ex = lookupVar(nm->name);
                if (ex) {
                    if (!isAssignable(ex, v))
                        error(n->loc, "cannot assign " + v->toString() +
                            " to '" + nm->name + "' of type " + ex->toString());
                }
                else defineVar(nm->name, v);
                return;
            }
            if (n->target->kind == ExprKind::Attr) {
                auto* a = static_cast<const AttrExpr*>(n->target.get());
                TypePtr t = checkExpr(a->target.get());
                if (t->kind != TypeKind::Struct)
                    error(a->loc, "cannot set field '" + a->name +
                        "' on value of type " + t->toString());
                const StructFieldInfo* f = t->findField(a->name);
                if (!f) error(a->loc, "type '" + t->name + "' has no field '" + a->name + "'");
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
            for (auto& ec : n->elifs) { checkExpr(ec.cond.get()); checkBlock(ec.body); }
            if (n->elseBody) checkBlock(*n->elseBody);
            return;
        }
        case StmtKind::While: {
            auto* n = static_cast<const WhileStmt*>(s);
            checkExpr(n->cond.get());
            ++loopDepth_; checkBlock(n->body); --loopDepth_;
            return;
        }
        case StmtKind::Def: {
            auto* n = static_cast<const DefStmt*>(s);
            auto it = functions_.find(n->name);
            if (it == functions_.end()) error(n->loc, "internal: missing signature");
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
                    " from function declared to return " + currentReturnType_->toString());
            return;
        }
        case StmtKind::Struct:
            return;

        case StmtKind::Class: {
            auto* n = static_cast<const ClassStmt*>(s);
            TypePtr ct = structs_.at(n->name);
            for (auto& m : n->methods) checkMethodBody(m.get(), ct);
            return;
        }

        case StmtKind::Pass: return;
        case StmtKind::Break:
        case StmtKind::Continue:
            if (loopDepth_ == 0)
                error(s->loc, s->kind == StmtKind::Break ? "'break' outside loop"
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
        case ExprKind::IntLit: return Types::Int();
        case ExprKind::FloatLit: return Types::Float();
        case ExprKind::StringLit: return Types::Str();
        case ExprKind::CharLit: return Types::Char();
        case ExprKind::BoolLit: return Types::Bool();
        case ExprKind::NoneLit: return Types::None();

        case ExprKind::NameRef: {
            auto* n = static_cast<const NameRefExpr*>(e);
            if (TypePtr t = lookupVar(n->name)) return t;
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
            case UnOp::Neg: case UnOp::Pos:
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
                checkExpr(n->lhs.get()); checkExpr(n->rhs.get());
                return Types::Bool();
            }
            TypePtr lt = checkExpr(n->lhs.get()), rt = checkExpr(n->rhs.get());
            if (lt->kind == TypeKind::Error || rt->kind == TypeKind::Error) return Types::Error();
            if (lt->kind == TypeKind::Any || rt->kind == TypeKind::Any) {
                switch (n->op) {
                case BinOp::Eq: case BinOp::NotEq:
                case BinOp::Lt: case BinOp::Gt:
                case BinOp::LtEq: case BinOp::GtEq:
                case BinOp::In: case BinOp::Is: return Types::Bool();
                default: return Types::Any();
                }
            }
            switch (n->op) {
            case BinOp::Add:
                if (lt->kind == TypeKind::Int && rt->kind == TypeKind::Int) return Types::Int();
                if (lt->kind == TypeKind::Float && rt->kind == TypeKind::Float) return Types::Float();
                if (lt->kind == TypeKind::Int && rt->kind == TypeKind::Float) return Types::Float();
                if (lt->kind == TypeKind::Float && rt->kind == TypeKind::Int) return Types::Float();
                if (lt->kind == TypeKind::Str && rt->kind == TypeKind::Str) return Types::Str();
                error(n->loc, "cannot add " + lt->toString() + " and " + rt->toString());
            case BinOp::Sub: {
                TypePtr c = commonNumeric(lt, rt);
                if (c->kind == TypeKind::Error)
                    error(n->loc, "cannot subtract " + rt->toString() + " from " + lt->toString());
                return c;
            }
            case BinOp::Mul:
                if (lt->kind == TypeKind::Str && rt->kind == TypeKind::Int) return Types::Str();
                if (lt->kind == TypeKind::Int && rt->kind == TypeKind::Str) return Types::Str();
                {
                    TypePtr c = commonNumeric(lt, rt);
                    if (c->kind == TypeKind::Error)
                        error(n->loc, "cannot multiply " + lt->toString() + " by " + rt->toString());
                    return c;
                }
            case BinOp::Div: {
                bool ln = lt->kind == TypeKind::Int || lt->kind == TypeKind::Float;
                bool rn = rt->kind == TypeKind::Int || rt->kind == TypeKind::Float;
                if (!ln || !rn) error(n->loc, "cannot divide " + lt->toString() + " by " + rt->toString());
                return Types::Float();
            }
            case BinOp::FloorDiv: case BinOp::Mod: case BinOp::Pow: {
                TypePtr c = commonNumeric(lt, rt);
                if (c->kind == TypeKind::Error)
                    error(n->loc, std::string("cannot apply '") + binOpName(n->op) +
                        "' to " + lt->toString() + " and " + rt->toString());
                return c;
            }
            case BinOp::Eq: case BinOp::NotEq:
            case BinOp::Lt: case BinOp::Gt:
            case BinOp::LtEq: case BinOp::GtEq:
            case BinOp::In: case BinOp::Is: return Types::Bool();
            default: return Types::Error();
            }
        }

        case ExprKind::Attr: {
            auto* n = static_cast<const AttrExpr*>(e);
            TypePtr t = checkExpr(n->target.get());
            if (t->kind == TypeKind::Error) return t;
            if (t->kind == TypeKind::Any)   return Types::Any();
            if (t->kind != TypeKind::Struct)
                error(n->loc, "cannot read field or method '" + n->name +
                    "' on value of type " + t->toString());
            if (const auto* f = t->findField(n->name)) return f->type;
            if (TypePtr m = t->findMethod(n->name)) {
                std::vector<TypePtr> bound(m->params.begin() + 1, m->params.end());
                return Types::Function(std::move(bound), m->returnType);
            }
            error(n->loc, "type '" + t->name + "' has no field or method '" + n->name + "'");
        }

        case ExprKind::Call: {
            auto* n = static_cast<const CallExpr*>(e);

            if (n->callee->kind == ExprKind::NameRef) {
                const auto* nm = static_cast<const NameRefExpr*>(n->callee.get());

                // super()
                if (nm->name == "super") {
                    if (!currentClass_) error(n->loc, "'super()' outside method");
                    if (!currentClass_->parent)
                        error(n->loc, "class '" + currentClass_->name + "' has no parent");
                    return currentClass_->parent;
                }

                // struct / class construction
                auto sit = structs_.find(nm->name);
                if (sit != structs_.end()) {
                    TypePtr ct = sit->second;

                    // class with __init__?
                    TypePtr initSig = ct->findMethod("__init__");
                    if (initSig) {
                        for (const auto& a : n->args)
                            if (!a.name.empty())
                                error(a.loc, "class '" + nm->name +
                                    "' __init__ does not accept keyword arguments");
                        if (n->args.size() != initSig->params.size() - 1)
                            error(n->loc, "class '" + nm->name + "' constructor expects " +
                                std::to_string(initSig->params.size() - 1) +
                                " argument(s), got " + std::to_string(n->args.size()));
                        for (size_t i = 0; i < n->args.size(); ++i) {
                            TypePtr at = checkExpr(n->args[i].value.get());
                            if (!isAssignable(initSig->params[i + 1], at))
                                error(n->args[i].loc, "argument " + std::to_string(i + 1) +
                                    ": expected " +
                                    initSig->params[i + 1]->toString() +
                                    ", got " + at->toString());
                        }
                        return ct;
                    }

                    // no __init__: match declared fields (own + inherited)
                    std::vector<StructFieldInfo> all;
                    for (TypePtr c = ct; c; c = c->parent)
                        for (auto& f : c->fields) all.push_back(f);

                    std::vector<bool> seen(all.size(), false);
                    size_t pos = 0;
                    for (const auto& arg : n->args) {
                        TypePtr at = checkExpr(arg.value.get());
                        if (arg.name.empty()) {
                            if (pos >= all.size())
                                error(arg.loc, "too many positional arguments");
                            if (!isAssignable(all[pos].type, at))
                                error(arg.loc, "field '" + all[pos].name +
                                    "' expects " + all[pos].type->toString() +
                                    ", got " + at->toString());
                            seen[pos] = true; ++pos;
                        }
                        else {
                            int idx = -1;
                            for (size_t i = 0; i < all.size(); ++i)
                                if (all[i].name == arg.name) { idx = (int)i; break; }
                            if (idx < 0) error(arg.loc, "no field '" + arg.name + "'");
                            if (seen[idx]) error(arg.loc, "field given twice");
                            if (!isAssignable(all[idx].type, at))
                                error(arg.loc, "field '" + arg.name + "' expects " +
                                    all[idx].type->toString() + ", got " +
                                    at->toString());
                            seen[idx] = true;
                        }
                    }
                    for (size_t i = 0; i < all.size(); ++i)
                        if (!seen[i]) error(n->loc, "missing field '" + all[i].name + "'");
                    return ct;
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
                ? static_cast<const NameRefExpr*>(n->callee.get()) : nullptr;
            if (nm && (nm->name == "print" || nm->name == "min" || nm->name == "max"))
                return callee->returnType ? callee->returnType : Types::None();

            if (argTypes.size() != callee->params.size())
                error(n->loc, "function expects " + std::to_string(callee->params.size()) +
                    " argument(s), got " + std::to_string(argTypes.size()));
            for (size_t i = 0; i < argTypes.size(); ++i)
                if (!isAssignable(callee->params[i], argTypes[i]))
                    error(n->args[i].loc, "argument " + std::to_string(i + 1) +
                        ": expected " + callee->params[i]->toString() +
                        ", got " + argTypes[i]->toString());
            return callee->returnType ? callee->returnType : Types::None();
        }

        case ExprKind::Index:
            error(e->loc, "indexing not yet supported");
        }
        return Types::Error();
    }

} // namespace nova