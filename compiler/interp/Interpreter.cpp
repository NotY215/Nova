#include "Interpreter.hpp"
#include <cmath>
#include <iostream>

namespace nova {

    struct ReturnSignal { Value value; };
    struct BreakSignal {};
    struct ContinueSignal {};

    namespace {
        struct EnvGuard {
            std::shared_ptr<Environment>& slot;
            std::shared_ptr<Environment>  saved;
            EnvGuard(std::shared_ptr<Environment>& s, std::shared_ptr<Environment> n)
                : slot(s), saved(std::move(s)) {
                slot = std::move(n);
            }
            ~EnvGuard() { slot = std::move(saved); }
        };
    } // namespace

    // ===========================================================================
    // Constructor
    // ===========================================================================

    Interpreter::Interpreter() {
        globals_ = std::make_shared<Environment>(nullptr);
        env_ = globals_;
        installBuiltins();
    }

    void Interpreter::run(const Block& program) {
        // First sweep: register all struct declarations so constructor calls
        // and forward references work regardless of order.
        for (auto& s : program.stmts) {
            if (s->kind == StmtKind::Struct) {
                auto* d = static_cast<const StructStmt*>(s.get());
                structDecls_[d->name] = d;
            }
        }
        execBlock(program);
    }

    void Interpreter::execBlock(const Block& b) {
        for (auto& s : b.stmts) exec(s.get());
    }

    // ===========================================================================
    // Statements
    // ===========================================================================

    void Interpreter::exec(const Stmt* s) {
        if (!s) return;
        switch (s->kind) {

        case StmtKind::Expr: {
            auto* n = static_cast<const ExprStmt*>(s);
            eval(n->expr.get());
            return;
        }

        case StmtKind::Assign: {
            execAssign(static_cast<const AssignStmt*>(s));
            return;
        }

        case StmtKind::AnnotAssign: {
            auto* n = static_cast<const AnnotAssignStmt*>(s);
            if (n->value) env_->define(n->name, eval(n->value.get()));
            else          env_->define(n->name, Value());
            return;
        }

        case StmtKind::If: {
            auto* n = static_cast<const IfStmt*>(s);
            if (eval(n->cond.get()).truthy()) { execBlock(n->thenBody); return; }
            for (auto& ec : n->elifs) {
                if (eval(ec.cond.get()).truthy()) { execBlock(ec.body); return; }
            }
            if (n->elseBody) execBlock(*n->elseBody);
            return;
        }

        case StmtKind::While: {
            auto* n = static_cast<const WhileStmt*>(s);
            while (eval(n->cond.get()).truthy()) {
                try { execBlock(n->body); }
                catch (BreakSignal&) { break; }
                catch (ContinueSignal&) { continue; }
            }
            return;
        }

        case StmtKind::Def: {
            auto* n = static_cast<const DefStmt*>(s);
            auto fn = std::make_shared<Callable>();
            fn->kind = Callable::Kind::User;
            fn->name = n->name;
            fn->decl = n;
            fn->closure = env_;
            env_->define(n->name, Value(fn));
            return;
        }

        case StmtKind::Return: {
            auto* n = static_cast<const ReturnStmt*>(s);
            ReturnSignal sig;
            if (n->value) sig.value = eval(n->value.get());
            throw sig;
        }

        case StmtKind::Struct:
            // Already registered in run().
            return;

        case StmtKind::Pass:     return;
        case StmtKind::Break:    throw BreakSignal{};
        case StmtKind::Continue: throw ContinueSignal{};
        }
    }

    void Interpreter::execAssign(const AssignStmt* n) {
        Value v = eval(n->value.get());

        if (n->target->kind == ExprKind::NameRef) {
            const auto* nm = static_cast<const NameRefExpr*>(n->target.get());
            if (!env_->assign(nm->name, v)) env_->define(nm->name, std::move(v));
            return;
        }

        if (n->target->kind == ExprKind::Attr) {
            auto* a = static_cast<const AttrExpr*>(n->target.get());
            Value inst = eval(a->target.get());
            if (!inst.isStruct())
                throw RuntimeError("cannot assign field on " + inst.typeName(), a->loc);
            auto si = inst.asStruct();
            if (si->fields.find(a->name) == si->fields.end())
                throw RuntimeError("struct '" + si->typeName +
                    "' has no field '" + a->name + "'", a->loc);
            si->fields[a->name] = std::move(v);
            return;
        }

        throw RuntimeError("invalid assignment target", n->target->loc);
    }

    // ===========================================================================
    // Expressions
    // ===========================================================================

    Value Interpreter::eval(const Expr* e) {
        if (!e) return Value();
        switch (e->kind) {
        case ExprKind::IntLit:    return Value(static_cast<const IntLitExpr*>(e)->value);
        case ExprKind::FloatLit:  return Value(static_cast<const FloatLitExpr*>(e)->value);
        case ExprKind::StringLit: return Value(static_cast<const StringLitExpr*>(e)->value);
        case ExprKind::CharLit:   return Value(static_cast<const CharLitExpr*>(e)->value);
        case ExprKind::BoolLit:   return Value(static_cast<const BoolLitExpr*>(e)->value);
        case ExprKind::NoneLit:   return Value();

        case ExprKind::NameRef: {
            auto* n = static_cast<const NameRefExpr*>(e);
            Value* slot = env_->lookup(n->name);
            if (slot) return *slot;

            // Struct used as constructor: make a "ctor" callable.
            auto it = structDecls_.find(n->name);
            if (it != structDecls_.end()) {
                auto c = std::make_shared<Callable>();
                c->kind = Callable::Kind::StructCtor;
                c->name = n->name;
                auto proto = std::make_shared<StructInstance>();
                proto->typeName = n->name;
                for (auto& f : it->second->fields) {
                    proto->fieldOrder.push_back(f.name);
                }
                c->proto = proto;
                return Value(c);
            }
            throw RuntimeError("name '" + n->name + "' is not defined", n->loc);
        }

        case ExprKind::Grouping:
            return eval(static_cast<const GroupingExpr*>(e)->inner.get());

        case ExprKind::Unary: {
            auto* n = static_cast<const UnaryExpr*>(e);
            Value v = eval(n->operand.get());
            switch (n->op) {
            case UnOp::Not: return Value(!v.truthy());
            case UnOp::Neg:
                if (v.isInt())   return Value(-v.asInt());
                if (v.isFloat()) return Value(-v.asFloat());
                throw RuntimeError("cannot negate " + v.typeName(), n->loc);
            case UnOp::Pos:
                if (v.isNumber()) return v;
                throw RuntimeError("cannot apply unary '+' to " + v.typeName(), n->loc);
            }
            return Value();
        }

        case ExprKind::Binary:
            return evalBinary(static_cast<const BinaryExpr*>(e));

        case ExprKind::Call:
            return evalCall(static_cast<const CallExpr*>(e));

        case ExprKind::Attr:
            return evalAttr(static_cast<const AttrExpr*>(e));

        case ExprKind::Index:
            throw RuntimeError("indexing not yet supported", e->loc);
        }
        return Value();
    }

    Value Interpreter::evalAttr(const AttrExpr* a) {
        Value base = eval(a->target.get());
        if (!base.isStruct())
            throw RuntimeError("cannot read field '" + a->name +
                "' on value of type " + base.typeName(), a->loc);
        auto si = base.asStruct();
        auto it = si->fields.find(a->name);
        if (it == si->fields.end())
            throw RuntimeError("struct '" + si->typeName +
                "' has no field '" + a->name + "'", a->loc);
        return it->second;
    }

    Value Interpreter::evalCall(const CallExpr* c) {
        // Optimisation / special-case: struct construction?
        if (c->callee->kind == ExprKind::NameRef) {
            const auto* nm = static_cast<const NameRefExpr*>(c->callee.get());
            auto it = structDecls_.find(nm->name);
            if (it != structDecls_.end()) {
                std::vector<std::pair<std::string, Value>> kwargs;
                for (auto& a : c->args) {
                    kwargs.emplace_back(a.name, eval(a.value.get()));
                }
                return constructStruct(nm->name, kwargs, c->loc);
            }
        }

        Value callee = eval(c->callee.get());

        // Struct ctor via callable path (e.g. from a variable).
        if (callee.isCallable() && callee.asCallable()->kind == Callable::Kind::StructCtor) {
            std::vector<std::pair<std::string, Value>> kwargs;
            for (auto& a : c->args) kwargs.emplace_back(a.name, eval(a.value.get()));
            return constructStruct(callee.asCallable()->name, kwargs, c->loc);
        }

        std::vector<Value> args;
        args.reserve(c->args.size());
        for (auto& a : c->args) {
            if (!a.name.empty())
                throw RuntimeError("function does not accept keyword arguments",
                    a.loc);
            args.push_back(eval(a.value.get()));
        }
        return callValue(callee, args, c->loc);
    }

    // ===========================================================================
    // Struct construction
    // ===========================================================================

    Value Interpreter::constructStruct(
        const std::string& name,
        const std::vector<std::pair<std::string, Value>>& args,
        SourceLocation loc)
    {
        auto it = structDecls_.find(name);
        if (it == structDecls_.end())
            throw RuntimeError("unknown struct '" + name + "'", loc);

        const StructStmt* d = it->second;
        auto si = std::make_shared<StructInstance>();
        si->typeName = name;

        for (auto& f : d->fields) si->fieldOrder.push_back(f.name);

        size_t positional = 0;
        for (const auto& [argName, argVal] : args) {
            if (argName.empty()) {
                if (positional >= d->fields.size())
                    throw RuntimeError("too many positional arguments for struct '" +
                        name + "'", loc);
                si->fields[d->fields[positional].name] = argVal;
                ++positional;
            }
            else {
                bool found = false;
                for (auto& f : d->fields)
                    if (f.name == argName) { found = true; break; }
                if (!found)
                    throw RuntimeError("struct '" + name + "' has no field '" +
                        argName + "'", loc);
                si->fields[argName] = argVal;
            }
        }
        return Value(si);
    }

    // ===========================================================================
    // Binary operators
    // ===========================================================================

    Value Interpreter::evalBinary(const BinaryExpr* b) {
        if (b->op == BinOp::And) {
            Value l = eval(b->lhs.get());
            return l.truthy() ? eval(b->rhs.get()) : l;
        }
        if (b->op == BinOp::Or) {
            Value l = eval(b->lhs.get());
            return l.truthy() ? l : eval(b->rhs.get());
        }

        Value l = eval(b->lhs.get());
        Value r = eval(b->rhs.get());

        auto numFail = [&]() -> void {
            throw RuntimeError(
                "cannot apply '" + std::string(binOpName(b->op)) + "' to " +
                l.typeName() + " and " + r.typeName(), b->loc);
            };

        if (b->op == BinOp::Eq || b->op == BinOp::NotEq) {
            bool eq = false;
            if (l.isNumber() && r.isNumber())      eq = l.asDouble() == r.asDouble();
            else if (l.isString() && r.isString()) eq = l.asString() == r.asString();
            else if (l.isBool() && r.isBool())     eq = l.asBool() == r.asBool();
            else if (l.isNone() && r.isNone())     eq = true;
            return Value(b->op == BinOp::Eq ? eq : !eq);
        }

        if (b->op == BinOp::Lt || b->op == BinOp::Gt ||
            b->op == BinOp::LtEq || b->op == BinOp::GtEq) {
            bool result = false;
            if (l.isNumber() && r.isNumber()) {
                double a = l.asDouble(), c = r.asDouble();
                switch (b->op) {
                case BinOp::Lt:   result = a < c; break;
                case BinOp::Gt:   result = a > c; break;
                case BinOp::LtEq: result = a <= c; break;
                case BinOp::GtEq: result = a >= c; break;
                default: break;
                }
            }
            else if (l.isString() && r.isString()) {
                const auto& a = l.asString(); const auto& c = r.asString();
                switch (b->op) {
                case BinOp::Lt:   result = a < c; break;
                case BinOp::Gt:   result = a > c; break;
                case BinOp::LtEq: result = a <= c; break;
                case BinOp::GtEq: result = a >= c; break;
                default: break;
                }
            }
            else numFail();
            return Value(result);
        }

        switch (b->op) {
        case BinOp::Add:
            if (l.isInt() && r.isInt())    return Value(l.asInt() + r.asInt());
            if (l.isNumber() && r.isNumber()) return Value(l.asDouble() + r.asDouble());
            if (l.isString() && r.isString()) return Value(l.asString() + r.asString());
            numFail();
        case BinOp::Sub:
            if (l.isInt() && r.isInt())    return Value(l.asInt() - r.asInt());
            if (l.isNumber() && r.isNumber()) return Value(l.asDouble() - r.asDouble());
            numFail();
        case BinOp::Mul: {
            if (l.isInt() && r.isInt())    return Value(l.asInt() * r.asInt());
            if (l.isNumber() && r.isNumber()) return Value(l.asDouble() * r.asDouble());
            if (l.isString() && r.isInt()) {
                std::string out;
                for (long long i = 0; i < r.asInt(); ++i) out += l.asString();
                return Value(std::move(out));
            }
            if (l.isInt() && r.isString()) {
                std::string out;
                for (long long i = 0; i < l.asInt(); ++i) out += r.asString();
                return Value(std::move(out));
            }
            numFail();
        }
        case BinOp::Div: {
            if (l.isNumber() && r.isNumber()) {
                double d = r.asDouble();
                if (d == 0.0) throw RuntimeError("division by zero", b->loc);
                return Value(l.asDouble() / d);
            }
            numFail();
        }
        case BinOp::FloorDiv: {
            if (l.isNumber() && r.isNumber()) {
                double d = r.asDouble();
                if (d == 0.0) throw RuntimeError("division by zero", b->loc);
                double q = std::floor(l.asDouble() / d);
                if (l.isInt() && r.isInt()) return Value((long long)q);
                return Value(q);
            }
            numFail();
        }
        case BinOp::Mod: {
            if (l.isNumber() && r.isNumber()) {
                double d = r.asDouble();
                if (d == 0.0) throw RuntimeError("modulo by zero", b->loc);
                double m = std::fmod(l.asDouble(), d);
                if (m != 0 && ((m < 0) != (d < 0))) m += d;
                if (l.isInt() && r.isInt()) return Value((long long)m);
                return Value(m);
            }
            numFail();
        }
        case BinOp::Pow: {
            if (l.isNumber() && r.isNumber()) {
                if (l.isInt() && r.isInt() && r.asInt() >= 0) {
                    long long base = l.asInt(), exp = r.asInt(), acc = 1;
                    while (exp--) acc *= base;
                    return Value(acc);
                }
                return Value(std::pow(l.asDouble(), r.asDouble()));
            }
            numFail();
        }
        case BinOp::In:
        case BinOp::Is:
            throw RuntimeError(std::string("operator '") + binOpName(b->op) +
                "' not yet supported", b->loc);
        default: break;
        }
        numFail();
        return Value();
    }

    // ===========================================================================
    // Calls
    // ===========================================================================

    Value Interpreter::callValue(const Value& callee,
        const std::vector<Value>& args,
        SourceLocation loc) {
        if (!callee.isCallable())
            throw RuntimeError("attempt to call " + callee.typeName() + " value", loc);
        auto fn = callee.asCallable();
        if (fn->kind == Callable::Kind::Native) return fn->nativeFn(args);
        if (fn->kind == Callable::Kind::StructCtor) {
            std::vector<std::pair<std::string, Value>> kwargs;
            for (auto& a : args) kwargs.emplace_back("", a);
            return constructStruct(fn->name, kwargs, loc);
        }
        return callUser(fn, args, loc);
    }

    Value Interpreter::callUser(const std::shared_ptr<Callable>& fn,
        const std::vector<Value>& args,
        SourceLocation loc) {
        const DefStmt* d = fn->decl;
        if (args.size() != d->params.size())
            throw RuntimeError("function '" + fn->name + "' expects " +
                std::to_string(d->params.size()) +
                " argument(s), got " + std::to_string(args.size()), loc);

        auto callEnv = std::make_shared<Environment>(fn->closure);
        for (size_t i = 0; i < args.size(); ++i)
            callEnv->define(d->params[i].name, args[i]);

        EnvGuard guard(env_, callEnv);
        try { execBlock(d->body); }
        catch (ReturnSignal& r) { return r.value; }
        return Value();
    }

    // ===========================================================================
    // Builtins (unchanged from Step 3A)
    // ===========================================================================

    namespace {
        Value bi_print(const std::vector<Value>& args) {
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) std::cout << ' ';
                std::cout << args[i].toString();
            }
            std::cout << '\n';
            return Value();
        }
        Value bi_str(const std::vector<Value>& a) { return a.empty() ? Value("") : Value(a[0].toString()); }
        Value bi_bool(const std::vector<Value>& a) { return a.empty() ? Value(false) : Value(a[0].truthy()); }
        Value bi_int(const std::vector<Value>& a) {
            if (a.empty()) return Value(0LL);
            const Value& v = a[0];
            if (v.isInt())    return v;
            if (v.isFloat())  return Value((long long)v.asFloat());
            if (v.isBool())   return Value((long long)(v.asBool() ? 1 : 0));
            if (v.isString()) {
                try { return Value((long long)std::stoll(v.asString())); }
                catch (...) { throw std::runtime_error("int(): invalid literal"); }
            }
            throw std::runtime_error("int(): cannot convert " + v.typeName());
        }
        Value bi_float(const std::vector<Value>& a) {
            if (a.empty()) return Value(0.0);
            const Value& v = a[0];
            if (v.isFloat()) return v;
            if (v.isInt())   return Value((double)v.asInt());
            if (v.isBool())  return Value(v.asBool() ? 1.0 : 0.0);
            if (v.isString()) {
                try { return Value(std::stod(v.asString())); }
                catch (...) { throw std::runtime_error("float(): invalid literal"); }
            }
            throw std::runtime_error("float(): cannot convert " + v.typeName());
        }
        Value bi_len(const std::vector<Value>& a) {
            if (a.size() != 1) throw std::runtime_error("len() takes exactly 1 argument");
            if (a[0].isString()) return Value((long long)a[0].asString().size());
            throw std::runtime_error("len(): unsupported type " + a[0].typeName());
        }
        Value bi_abs(const std::vector<Value>& a) {
            if (a.size() != 1 || !a[0].isNumber())
                throw std::runtime_error("abs() expects one number");
            if (a[0].isInt()) return Value(std::llabs(a[0].asInt()));
            return Value(std::fabs(a[0].asFloat()));
        }
        Value bi_type(const std::vector<Value>& a) {
            if (a.size() != 1) throw std::runtime_error("type() takes 1 argument");
            return Value(a[0].typeName());
        }
        Value bi_min(const std::vector<Value>& a) {
            if (a.empty()) throw std::runtime_error("min() requires at least one argument");
            Value best = a[0];
            for (size_t i = 1; i < a.size(); ++i) {
                if (a[i].isNumber() && best.isNumber()) {
                    if (a[i].asDouble() < best.asDouble()) best = a[i];
                }
                else if (a[i].isString() && best.isString()) {
                    if (a[i].asString() < best.asString()) best = a[i];
                }
                else throw std::runtime_error("min(): mixed types");
            }
            return best;
        }
        Value bi_max(const std::vector<Value>& a) {
            if (a.empty()) throw std::runtime_error("max() requires at least one argument");
            Value best = a[0];
            for (size_t i = 1; i < a.size(); ++i) {
                if (a[i].isNumber() && best.isNumber()) {
                    if (a[i].asDouble() > best.asDouble()) best = a[i];
                }
                else if (a[i].isString() && best.isString()) {
                    if (a[i].asString() > best.asString()) best = a[i];
                }
                else throw std::runtime_error("max(): mixed types");
            }
            return best;
        }
    } // namespace

    void Interpreter::installBuiltins() {
        auto add = [&](const char* name, NativeFnPtr fn) {
            auto c = std::make_shared<Callable>();
            c->kind = Callable::Kind::Native;
            c->name = name;
            c->nativeFn = fn;
            globals_->define(name, Value(c));
            };
        add("print", bi_print);
        add("str", bi_str);
        add("bool", bi_bool);
        add("int", bi_int);
        add("float", bi_float);
        add("len", bi_len);
        add("abs", bi_abs);
        add("type", bi_type);
        add("min", bi_min);
        add("max", bi_max);
    }

} // namespace nova