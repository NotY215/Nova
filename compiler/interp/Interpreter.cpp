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

        // Value equality used by `in`, `.contains`, `.remove`, `.index`.
        bool valueEquals(const Value& a, const Value& b) {
            if (a.isNumber() && b.isNumber()) return a.asDouble() == b.asDouble();
            if (a.isString() && b.isString()) return a.asString() == b.asString();
            if (a.isBool() && b.isBool())   return a.asBool() == b.asBool();
            if (a.isNone() && b.isNone())   return true;
            if (a.isList() && b.isList())   return a.asList() == b.asList();
            if (a.isMap() && b.isMap())    return a.asMap() == b.asMap();
            if (a.isInstance() && b.isInstance()) return a.asInstance() == b.asInstance();
            return false;
        }
    } // namespace

    // ===========================================================================
    // Constructor / registry
    // ===========================================================================

    Interpreter::Interpreter() {
        globals_ = std::make_shared<Environment>(nullptr);
        env_ = globals_;
        installBuiltins();
    }

    void Interpreter::run(const Block& program) {
        for (auto& s : program.stmts) {
            if (s->kind == StmtKind::Struct) registerStruct(static_cast<const StructStmt*>(s.get()));
            if (s->kind == StmtKind::Class)  registerClass(static_cast<const ClassStmt*>(s.get()));
        }
        execBlock(program);
    }

    void Interpreter::registerStruct(const StructStmt* d) {
        auto c = std::make_shared<ClassObject>();
        c->name = d->name;
        for (auto& f : d->fields) c->fieldOrder.push_back(f.name);
        classes_[d->name] = c;
    }
    void Interpreter::registerClass(const ClassStmt* d) {
        auto c = std::make_shared<ClassObject>();
        c->name = d->name;
        for (auto& f : d->fields) c->fieldOrder.push_back(f.name);
        if (!d->parentName.empty()) {
            auto p = classes_.find(d->parentName);
            if (p == classes_.end())
                throw RuntimeError("unknown parent class '" + d->parentName + "'", d->loc);
            c->parent = p->second;
        }
        classes_[d->name] = c;
        classDecls_[d->name] = d;
    }

    // ===========================================================================
    // Statement execution
    // ===========================================================================

    void Interpreter::execBlock(const Block& b) {
        for (auto& s : b.stmts) exec(s.get());
    }

    void Interpreter::exec(const Stmt* s) {
        if (!s) return;
        switch (s->kind) {
        case StmtKind::Expr:
            eval(static_cast<const ExprStmt*>(s)->expr.get()); return;
        case StmtKind::Assign:
            execAssign(static_cast<const AssignStmt*>(s)); return;
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
        case StmtKind::For:
            execFor(static_cast<const ForStmt*>(s)); return;

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
        case StmtKind::Class:
            return;   // handled in run()

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
            if (!inst.isInstance())
                throw RuntimeError("cannot assign field on " + inst.typeName(), a->loc);
            auto si = inst.asInstance();
            si->fields[a->name] = std::move(v);
            return;
        }

        if (n->target->kind == ExprKind::Index) {
            auto* ix = static_cast<const IndexExpr*>(n->target.get());
            Value tgt = eval(ix->target.get());
            Value idx = eval(ix->index.get());

            if (tgt.isList()) {
                if (!idx.isInt())
                    throw RuntimeError("list index must be int, got " + idx.typeName(), ix->loc);
                auto lst = tgt.asList();
                long long i = idx.asInt();
                if (i < 0) i += (long long)lst->items.size();
                if (i < 0 || i >= (long long)lst->items.size())
                    throw RuntimeError("list index out of range", ix->loc);
                lst->items[(size_t)i] = std::move(v);
                return;
            }
            if (tgt.isMap()) {
                if (!idx.isString())
                    throw RuntimeError("map key must be str, got " + idx.typeName(), ix->loc);
                tgt.asMap()->entries[idx.asString()] = std::move(v);
                return;
            }
            if (tgt.isString())
                throw RuntimeError("strings are immutable", ix->loc);
            throw RuntimeError("cannot index-assign to value of type " + tgt.typeName(), ix->loc);
        }

        throw RuntimeError("invalid assignment target", n->target->loc);
    }

    void Interpreter::execFor(const ForStmt* n) {
        Value iterable = eval(n->iterable.get());

        auto bindVar = [&](Value v) {
            if (!env_->assign(n->targetName, v))
                env_->define(n->targetName, std::move(v));
            };
        auto runBody = [&]() -> bool {   // true = continue, false = break
            try { execBlock(n->body); }
            catch (BreakSignal&) { return false; }
            catch (ContinueSignal&) { return true; }
            return true;
            };

        if (iterable.isList()) {
            auto lst = iterable.asList();
            size_t count = lst->items.size();          // snapshot count
            for (size_t i = 0; i < count; ++i) {
                if (i >= lst->items.size()) break;     // shrunk mid-loop
                bindVar(lst->items[i]);
                if (!runBody()) break;
            }
            return;
        }
        if (iterable.isMap()) {
            auto m = iterable.asMap();
            std::vector<std::string> keys;             // snapshot keys
            keys.reserve(m->entries.size());
            for (auto& [k, _] : m->entries) keys.push_back(k);
            for (auto& k : keys) {
                bindVar(Value(k));
                if (!runBody()) break;
            }
            return;
        }
        if (iterable.isString()) {
            const std::string& s = iterable.asString();
            for (size_t i = 0; i < s.size(); ++i) {
                bindVar(Value(std::string(1, s[i])));
                if (!runBody()) break;
            }
            return;
        }
        throw RuntimeError("cannot iterate over value of type " + iterable.typeName(), n->loc);
    }

    // ===========================================================================
    // Expression evaluation
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
            if (Value* slot = env_->lookup(n->name)) return *slot;
            auto it = classes_.find(n->name);
            if (it != classes_.end()) {
                auto c = std::make_shared<Callable>();
                c->kind = Callable::Kind::ClassCtor;
                c->name = n->name;
                c->classObj = it->second;
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

        case ExprKind::Binary: return evalBinary(static_cast<const BinaryExpr*>(e));
        case ExprKind::Call:   return evalCall(static_cast<const CallExpr*>(e));
        case ExprKind::Attr:   return evalAttr(static_cast<const AttrExpr*>(e));
        case ExprKind::Index:  return evalIndex(static_cast<const IndexExpr*>(e));
        case ExprKind::ListLit:return evalListLit(static_cast<const ListLitExpr*>(e));
        case ExprKind::MapLit: return evalMapLit(static_cast<const MapLitExpr*>(e));

        case ExprKind::GenericType:
            throw RuntimeError("generic type expression used as a value", e->loc);
        }
        return Value();
    }

    Value Interpreter::evalListLit(const ListLitExpr* n) {
        auto lst = std::make_shared<ListValue>();
        lst->items.reserve(n->elements.size());
        for (auto& el : n->elements) lst->items.push_back(eval(el.get()));
        return Value(lst);
    }

    Value Interpreter::evalMapLit(const MapLitExpr* n) {
        auto m = std::make_shared<MapValue>();
        for (auto& entry : n->entries) {
            Value k = eval(entry.key.get());
            if (!k.isString())
                throw RuntimeError("map keys must be str, got " + k.typeName(),
                    entry.key->loc);
            m->entries[k.asString()] = eval(entry.value.get());
        }
        return Value(m);
    }

    Value Interpreter::evalIndex(const IndexExpr* ix) {
        Value tgt = eval(ix->target.get());
        Value idx = eval(ix->index.get());

        if (tgt.isList()) {
            if (!idx.isInt())
                throw RuntimeError("list index must be int, got " + idx.typeName(), ix->loc);
            auto lst = tgt.asList();
            long long i = idx.asInt();
            if (i < 0) i += (long long)lst->items.size();
            if (i < 0 || i >= (long long)lst->items.size())
                throw RuntimeError("list index out of range", ix->loc);
            return lst->items[(size_t)i];
        }
        if (tgt.isMap()) {
            if (!idx.isString())
                throw RuntimeError("map key must be str, got " + idx.typeName(), ix->loc);
            auto m = tgt.asMap();
            auto it = m->entries.find(idx.asString());
            if (it == m->entries.end())
                throw RuntimeError("map has no key '" + idx.asString() + "'", ix->loc);
            return it->second;
        }
        if (tgt.isString()) {
            if (!idx.isInt())
                throw RuntimeError("str index must be int, got " + idx.typeName(), ix->loc);
            const std::string& s = tgt.asString();
            long long i = idx.asInt();
            if (i < 0) i += (long long)s.size();
            if (i < 0 || i >= (long long)s.size())
                throw RuntimeError("string index out of range", ix->loc);
            return Value(std::string(1, s[(size_t)i]));
        }
        throw RuntimeError("cannot index value of type " + tgt.typeName(), ix->loc);
    }

    Value Interpreter::evalAttr(const AttrExpr* a) {
        Value base = eval(a->target.get());

        // super() proxy
        if (base.isCallable() && base.asCallable()->kind == Callable::Kind::SuperMethod) {
            auto sp = base.asCallable();
            std::shared_ptr<ClassObject> dummy;
            auto m = findMethod(sp->superParent, a->name, &dummy);
            if (!m) throw RuntimeError("parent class has no method '" + a->name + "'", a->loc);
            auto bm = std::make_shared<Callable>();
            bm->kind = Callable::Kind::BoundMethod;
            bm->name = a->name;
            bm->boundSelf = sp->boundSelf;
            bm->methodFn = m;
            bm->definingClass = dummy;
            return Value(bm);
        }

        // List methods
        if (base.isList()) {
            auto c = std::make_shared<Callable>();
            c->kind = Callable::Kind::ListMethod;
            c->name = a->name;
            c->boundList = base.asList();
            return Value(c);
        }
        // Map methods
        if (base.isMap()) {
            auto c = std::make_shared<Callable>();
            c->kind = Callable::Kind::MapMethod;
            c->name = a->name;
            c->boundMap = base.asMap();
            return Value(c);
        }

        if (!base.isInstance())
            throw RuntimeError("cannot read '" + a->name + "' on value of type " +
                base.typeName(), a->loc);
        auto si = base.asInstance();

        auto fit = si->fields.find(a->name);
        if (fit != si->fields.end()) return fit->second;

        if (si->cls) {
            std::shared_ptr<ClassObject> defCls;
            auto m = findMethod(si->cls, a->name, &defCls);
            if (m) {
                auto bm = std::make_shared<Callable>();
                bm->kind = Callable::Kind::BoundMethod;
                bm->name = a->name;
                bm->boundSelf = si;
                bm->methodFn = m;
                bm->definingClass = defCls;
                return Value(bm);
            }
        }
        throw RuntimeError("type '" + (si->cls ? si->cls->name : "?") +
            "' has no field or method '" + a->name + "'", a->loc);
    }

    std::shared_ptr<Callable> Interpreter::findMethod(
        const std::shared_ptr<ClassObject>& cls,
        const std::string& name,
        std::shared_ptr<ClassObject>* definingClass)
    {
        for (auto c = cls; c; c = c->parent) {
            auto it = classDecls_.find(c->name);
            if (it == classDecls_.end()) continue;
            for (auto& m : it->second->methods) {
                if (m->name == name) {
                    auto fn = std::make_shared<Callable>();
                    fn->kind = Callable::Kind::User;
                    fn->name = name;
                    fn->decl = m.get();
                    if (definingClass) *definingClass = c;
                    return fn;
                }
            }
        }
        return nullptr;
    }

    Value Interpreter::evalCall(const CallExpr* c) {
        // super() special case
        if (c->callee->kind == ExprKind::NameRef) {
            const auto* nm = static_cast<const NameRefExpr*>(c->callee.get());
            if (nm->name == "super") {
                if (!c->args.empty())
                    throw RuntimeError("super() takes no arguments", c->loc);
                Value* s = env_->lookup("self");
                Value* k = env_->lookup("__class__");
                if (!s || !k || !s->isInstance() || !k->isClass())
                    throw RuntimeError("super() outside method", c->loc);
                auto sup = std::make_shared<Callable>();
                sup->kind = Callable::Kind::SuperMethod;
                sup->name = "super";
                sup->boundSelf = s->asInstance();
                sup->superParent = k->asClass()->parent;
                if (!sup->superParent)
                    throw RuntimeError("class '" + k->asClass()->name +
                        "' has no parent", c->loc);
                return Value(sup);
            }
        }

        Value callee = eval(c->callee.get());

        if (callee.isCallable() && callee.asCallable()->kind == Callable::Kind::ClassCtor) {
            std::vector<std::pair<std::string, Value>> kwargs;
            for (auto& a : c->args) kwargs.emplace_back(a.name, eval(a.value.get()));
            return constructInstance(callee.asCallable()->classObj, kwargs, c->loc);
        }
        if (callee.isCallable() && callee.asCallable()->kind == Callable::Kind::SuperMethod)
            throw RuntimeError("cannot call super() directly", c->loc);

        std::vector<Value> args;
        args.reserve(c->args.size());
        for (auto& a : c->args) {
            if (!a.name.empty())
                throw RuntimeError("function does not accept keyword arguments", a.loc);
            args.push_back(eval(a.value.get()));
        }
        return callValue(callee, args, c->loc);
    }

    // ===========================================================================
    // Instance construction
    // ===========================================================================

    Value Interpreter::constructInstance(
        const std::shared_ptr<ClassObject>& cls,
        const std::vector<std::pair<std::string, Value>>& args,
        SourceLocation loc)
    {
        auto inst = std::make_shared<StructInstance>();
        inst->cls = cls;

        const DefStmt* initDecl = nullptr;
        std::shared_ptr<ClassObject> initOwner;
        for (auto c = cls; c; c = c->parent) {
            auto it = classDecls_.find(c->name);
            if (it == classDecls_.end()) continue;
            for (auto& m : it->second->methods) {
                if (m->name == "__init__") { initDecl = m.get(); initOwner = c; break; }
            }
            if (initDecl) break;
        }

        if (initDecl) {
            std::vector<Value> callArgs;
            callArgs.push_back(Value(inst));
            for (auto& [k, v] : args) {
                if (!k.empty())
                    throw RuntimeError("__init__ does not accept keyword arguments", loc);
                callArgs.push_back(v);
            }
            auto fn = std::make_shared<Callable>();
            fn->kind = Callable::Kind::User;
            fn->name = "__init__";
            fn->decl = initDecl;
            fn->closure = env_;
            fn->definingClass = initOwner;
            callUser(fn, callArgs, loc);
            return Value(inst);
        }

        std::vector<std::string> allFields;
        for (auto c = cls; c; c = c->parent)
            for (auto& f : c->fieldOrder) allFields.push_back(f);

        size_t positional = 0;
        for (auto& [k, v] : args) {
            if (k.empty()) {
                if (positional >= allFields.size())
                    throw RuntimeError("too many positional arguments", loc);
                inst->fields[allFields[positional]] = v;
                ++positional;
            }
            else {
                bool found = false;
                for (auto& f : allFields) if (f == k) { found = true; break; }
                if (!found)
                    throw RuntimeError("class '" + cls->name + "' has no field '" + k + "'", loc);
                inst->fields[k] = v;
            }
        }
        for (auto& f : allFields)
            if (!inst->fields.count(f))
                throw RuntimeError("class '" + cls->name +
                    "' is missing value for field '" + f + "'", loc);

        return Value(inst);
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

        // ---- `in` ----
        if (b->op == BinOp::In) {
            Value l = eval(b->lhs.get());
            Value r = eval(b->rhs.get());
            if (r.isList()) {
                for (auto& v : r.asList()->items)
                    if (valueEquals(l, v)) return Value(true);
                return Value(false);
            }
            if (r.isMap()) {
                if (!l.isString()) return Value(false);
                return Value(r.asMap()->entries.count(l.asString()) > 0);
            }
            if (r.isString() && l.isString())
                return Value(r.asString().find(l.asString()) != std::string::npos);
            throw RuntimeError("'in' requires a list, map, or str on the right", b->loc);
        }

        Value l = eval(b->lhs.get());
        Value r = eval(b->rhs.get());

        auto numFail = [&]() -> void {
            throw RuntimeError(std::string("cannot apply '") + binOpName(b->op) +
                "' to " + l.typeName() + " and " + r.typeName(), b->loc);
            };

        // ---- equality ----
        if (b->op == BinOp::Eq || b->op == BinOp::NotEq) {
            bool eq = valueEquals(l, r);
            return Value(b->op == BinOp::Eq ? eq : !eq);
        }

        // ---- comparison ----
        if (b->op == BinOp::Lt || b->op == BinOp::Gt ||
            b->op == BinOp::LtEq || b->op == BinOp::GtEq) {
            bool res = false;
            if (l.isNumber() && r.isNumber()) {
                double a = l.asDouble(), c = r.asDouble();
                switch (b->op) {
                case BinOp::Lt: res = a < c; break;
                case BinOp::Gt: res = a > c; break;
                case BinOp::LtEq: res = a <= c; break;
                case BinOp::GtEq: res = a >= c; break;
                default: break;
                }
            }
            else if (l.isString() && r.isString()) {
                const auto& a = l.asString(); const auto& c = r.asString();
                switch (b->op) {
                case BinOp::Lt: res = a < c; break;
                case BinOp::Gt: res = a > c; break;
                case BinOp::LtEq: res = a <= c; break;
                case BinOp::GtEq: res = a >= c; break;
                default: break;
                }
            }
            else numFail();
            return Value(res);
        }

        switch (b->op) {
        case BinOp::Add:
            if (l.isInt() && r.isInt()) return Value(l.asInt() + r.asInt());
            if (l.isNumber() && r.isNumber()) return Value(l.asDouble() + r.asDouble());
            if (l.isString() && r.isString()) return Value(l.asString() + r.asString());
            if (l.isList() && r.isList()) {
                auto out = std::make_shared<ListValue>();
                out->items = l.asList()->items;
                for (auto& v : r.asList()->items) out->items.push_back(v);
                return Value(out);
            }
            numFail();

        case BinOp::Sub:
            if (l.isInt() && r.isInt()) return Value(l.asInt() - r.asInt());
            if (l.isNumber() && r.isNumber()) return Value(l.asDouble() - r.asDouble());
            numFail();

        case BinOp::Mul: {
            if (l.isInt() && r.isInt()) return Value(l.asInt() * r.asInt());
            if (l.isNumber() && r.isNumber()) return Value(l.asDouble() * r.asDouble());
            if (l.isString() && r.isInt()) {
                std::string o;
                for (long long i = 0; i < r.asInt(); ++i) o += l.asString();
                return Value(std::move(o));
            }
            if (l.isInt() && r.isString()) {
                std::string o;
                for (long long i = 0; i < l.asInt(); ++i) o += r.asString();
                return Value(std::move(o));
            }
            if (l.isList() && r.isInt()) {
                auto out = std::make_shared<ListValue>();
                for (long long i = 0; i < r.asInt(); ++i)
                    for (auto& v : l.asList()->items) out->items.push_back(v);
                return Value(out);
            }
            if (l.isInt() && r.isList()) {
                auto out = std::make_shared<ListValue>();
                for (long long i = 0; i < l.asInt(); ++i)
                    for (auto& v : r.asList()->items) out->items.push_back(v);
                return Value(out);
            }
            numFail();
        }

        case BinOp::Div: {
            if (l.isNumber() && r.isNumber()) {
                double d = r.asDouble();
                if (d == 0.0) throw RuntimeError("division by zero", b->loc);
                return Value(l.asDouble() / d);
            } numFail();
        }

        case BinOp::FloorDiv: {
            if (l.isNumber() && r.isNumber()) {
                double d = r.asDouble();
                if (d == 0.0) throw RuntimeError("division by zero", b->loc);
                double q = std::floor(l.asDouble() / d);
                if (l.isInt() && r.isInt()) return Value((long long)q);
                return Value(q);
            } numFail();
        }

        case BinOp::Mod: {
            if (l.isNumber() && r.isNumber()) {
                double d = r.asDouble();
                if (d == 0.0) throw RuntimeError("modulo by zero", b->loc);
                double m = std::fmod(l.asDouble(), d);
                if (m != 0 && ((m < 0) != (d < 0))) m += d;
                if (l.isInt() && r.isInt()) return Value((long long)m);
                return Value(m);
            } numFail();
        }

        case BinOp::Pow: {
            if (l.isNumber() && r.isNumber()) {
                if (l.isInt() && r.isInt() && r.asInt() >= 0) {
                    long long base = l.asInt(), exp = r.asInt(), acc = 1;
                    while (exp--) acc *= base;
                    return Value(acc);
                }
                return Value(std::pow(l.asDouble(), r.asDouble()));
            } numFail();
        }

        case BinOp::Is:
            throw RuntimeError("'is' not yet supported", b->loc);
        default: break;
        }
        numFail(); return Value();
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
        switch (fn->kind) {
        case Callable::Kind::Native:   return fn->nativeFn(args);

        case Callable::Kind::ClassCtor: {
            std::vector<std::pair<std::string, Value>> kwargs;
            for (auto& a : args) kwargs.emplace_back("", a);
            return constructInstance(fn->classObj, kwargs, loc);
        }

        case Callable::Kind::User:      return callUser(fn, args, loc);

        case Callable::Kind::BoundMethod: {
            std::vector<Value> all;
            all.reserve(args.size() + 1);
            all.push_back(Value(fn->boundSelf));
            for (auto& a : args) all.push_back(a);
            auto inner = std::make_shared<Callable>(*fn->methodFn);
            inner->definingClass = fn->definingClass;
            inner->closure = globals_;
            return callUser(inner, all, loc);
        }

        case Callable::Kind::SuperMethod:
            throw RuntimeError("cannot call super() directly", loc);

        case Callable::Kind::ListMethod: return callListMethod(fn, args, loc);
        case Callable::Kind::MapMethod:  return callMapMethod(fn, args, loc);
        }
        return Value();
    }

    Value Interpreter::callListMethod(const std::shared_ptr<Callable>& fn,
        const std::vector<Value>& args,
        SourceLocation loc) {
        auto lst = fn->boundList;
        const std::string& m = fn->name;

        if (m == "append") {
            if (args.size() != 1) throw RuntimeError("append() takes 1 argument", loc);
            lst->items.push_back(args[0]);
            return Value();
        }
        if (m == "pop") {
            if (args.empty()) {
                if (lst->items.empty()) throw RuntimeError("pop from empty list", loc);
                Value v = lst->items.back();
                lst->items.pop_back();
                return v;
            }
            if (args.size() != 1 || !args[0].isInt())
                throw RuntimeError("pop() takes 0 or 1 int argument", loc);
            long long i = args[0].asInt();
            if (i < 0) i += (long long)lst->items.size();
            if (i < 0 || i >= (long long)lst->items.size())
                throw RuntimeError("pop index out of range", loc);
            Value v = lst->items[(size_t)i];
            lst->items.erase(lst->items.begin() + i);
            return v;
        }
        if (m == "clear") {
            if (!args.empty()) throw RuntimeError("clear() takes no arguments", loc);
            lst->items.clear();
            return Value();
        }
        if (m == "insert") {
            if (args.size() != 2 || !args[0].isInt())
                throw RuntimeError("insert(index, value) takes (int, T)", loc);
            long long i = args[0].asInt();
            if (i < 0) i += (long long)lst->items.size();
            if (i < 0) i = 0;
            if (i > (long long)lst->items.size()) i = (long long)lst->items.size();
            lst->items.insert(lst->items.begin() + i, args[1]);
            return Value();
        }
        if (m == "remove") {
            if (args.size() != 1) throw RuntimeError("remove() takes 1 argument", loc);
            for (size_t i = 0; i < lst->items.size(); ++i) {
                if (valueEquals(lst->items[i], args[0])) {
                    lst->items.erase(lst->items.begin() + i);
                    return Value();
                }
            }
            throw RuntimeError("remove(): value not in list", loc);
        }
        if (m == "contains") {
            if (args.size() != 1) throw RuntimeError("contains() takes 1 argument", loc);
            for (auto& v : lst->items)
                if (valueEquals(v, args[0])) return Value(true);
            return Value(false);
        }
        if (m == "index") {
            if (args.size() != 1) throw RuntimeError("index() takes 1 argument", loc);
            for (size_t i = 0; i < lst->items.size(); ++i)
                if (valueEquals(lst->items[i], args[0]))
                    return Value((long long)i);
            throw RuntimeError("index(): value not in list", loc);
        }
        throw RuntimeError("list has no method '" + m + "'", loc);
    }

    Value Interpreter::callMapMethod(const std::shared_ptr<Callable>& fn,
        const std::vector<Value>& args,
        SourceLocation loc) {
        auto m = fn->boundMap;
        const std::string& name = fn->name;

        if (name == "put") {
            if (args.size() != 2 || !args[0].isString())
                throw RuntimeError("put(key, value) takes (str, T)", loc);
            m->entries[args[0].asString()] = args[1];
            return Value();
        }
        if (name == "get") {
            if (args.size() != 1 || !args[0].isString())
                throw RuntimeError("get(key) takes one str argument", loc);
            auto it = m->entries.find(args[0].asString());
            if (it == m->entries.end())
                throw RuntimeError("map has no key '" + args[0].asString() + "'", loc);
            return it->second;
        }
        if (name == "remove") {
            if (args.size() != 1 || !args[0].isString())
                throw RuntimeError("remove(key) takes one str argument", loc);
            auto it = m->entries.find(args[0].asString());
            if (it == m->entries.end())
                throw RuntimeError("map has no key '" + args[0].asString() + "'", loc);
            m->entries.erase(it);
            return Value();
        }
        if (name == "contains") {
            if (args.size() != 1 || !args[0].isString())
                throw RuntimeError("contains(key) takes one str argument", loc);
            return Value(m->entries.count(args[0].asString()) > 0);
        }
        if (name == "keys") {
            if (!args.empty()) throw RuntimeError("keys() takes no arguments", loc);
            auto lst = std::make_shared<ListValue>();
            for (auto& [k, _] : m->entries) lst->items.push_back(Value(k));
            return Value(lst);
        }
        if (name == "values") {
            if (!args.empty()) throw RuntimeError("values() takes no arguments", loc);
            auto lst = std::make_shared<ListValue>();
            for (auto& [_, v] : m->entries) lst->items.push_back(v);
            return Value(lst);
        }
        if (name == "clear") {
            if (!args.empty()) throw RuntimeError("clear() takes no arguments", loc);
            m->entries.clear();
            return Value();
        }
        throw RuntimeError("map has no method '" + name + "'", loc);
    }

    Value Interpreter::callUser(const std::shared_ptr<Callable>& fn,
        const std::vector<Value>& args,
        SourceLocation loc) {
        const DefStmt* d = fn->decl;
        if (args.size() != d->params.size())
            throw RuntimeError("function '" + fn->name + "' expects " +
                std::to_string(d->params.size()) + " argument(s), got " +
                std::to_string(args.size()), loc);

        auto callEnv = std::make_shared<Environment>(fn->closure ? fn->closure : globals_);
        for (size_t i = 0; i < args.size(); ++i)
            callEnv->define(d->params[i].name, args[i]);

        if (fn->definingClass)
            callEnv->define("__class__", Value(fn->definingClass));

        EnvGuard guard(env_, callEnv);
        try { execBlock(d->body); }
        catch (ReturnSignal& r) { return r.value; }
        return Value();
    }

    // ===========================================================================
    // Builtins
    // ===========================================================================

    namespace {

        Value bi_print(const std::vector<Value>& args) {
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) std::cout << ' ';
                std::cout << args[i].toString();
            }
            std::cout << '\n'; return Value();
        }
        Value bi_str(const std::vector<Value>& a) { return a.empty() ? Value("") : Value(a[0].toString()); }
        Value bi_bool(const std::vector<Value>& a) { return a.empty() ? Value(false) : Value(a[0].truthy()); }
        Value bi_int(const std::vector<Value>& a) {
            if (a.empty()) return Value(0LL);
            const Value& v = a[0];
            if (v.isInt())    return v;
            if (v.isFloat())  return Value((long long)v.asFloat());
            if (v.isBool())   return Value((long long)(v.asBool() ? 1 : 0));
            if (v.isString()) { try { return Value((long long)std::stoll(v.asString())); } catch (...) {} }
            throw std::runtime_error("int(): cannot convert " + v.typeName());
        }
        Value bi_float(const std::vector<Value>& a) {
            if (a.empty()) return Value(0.0);
            const Value& v = a[0];
            if (v.isFloat()) return v;
            if (v.isInt())   return Value((double)v.asInt());
            if (v.isBool())  return Value(v.asBool() ? 1.0 : 0.0);
            if (v.isString()) { try { return Value(std::stod(v.asString())); } catch (...) {} }
            throw std::runtime_error("float(): cannot convert " + v.typeName());
        }
        Value bi_len(const std::vector<Value>& a) {
            if (a.size() != 1) throw std::runtime_error("len() takes exactly 1 argument");
            if (a[0].isString()) return Value((long long)a[0].asString().size());
            if (a[0].isList())   return Value((long long)a[0].asList()->items.size());
            if (a[0].isMap())    return Value((long long)a[0].asMap()->entries.size());
            throw std::runtime_error("len(): unsupported type " + a[0].typeName());
        }
        Value bi_abs(const std::vector<Value>& a) {
            if (a.size() != 1 || !a[0].isNumber()) throw std::runtime_error("abs() expects number");
            if (a[0].isInt()) return Value(std::llabs(a[0].asInt()));
            return Value(std::fabs(a[0].asFloat()));
        }
        Value bi_type(const std::vector<Value>& a) {
            if (a.size() != 1) throw std::runtime_error("type() takes 1 argument");
            return Value(a[0].typeName());
        }
        Value bi_min(const std::vector<Value>& a) {
            if (a.empty()) throw std::runtime_error("min() requires at least one argument");
            Value b = a[0];
            for (size_t i = 1; i < a.size(); ++i) {
                if (a[i].isNumber() && b.isNumber()) { if (a[i].asDouble() < b.asDouble()) b = a[i]; }
                else if (a[i].isString() && b.isString()) { if (a[i].asString() < b.asString()) b = a[i]; }
                else throw std::runtime_error("min(): mixed types");
            } return b;
        }
        Value bi_max(const std::vector<Value>& a) {
            if (a.empty()) throw std::runtime_error("max() requires at least one argument");
            Value b = a[0];
            for (size_t i = 1; i < a.size(); ++i) {
                if (a[i].isNumber() && b.isNumber()) { if (a[i].asDouble() > b.asDouble()) b = a[i]; }
                else if (a[i].isString() && b.isString()) { if (a[i].asString() > b.asString()) b = a[i]; }
                else throw std::runtime_error("max(): mixed types");
            } return b;
        }
        Value bi_range(const std::vector<Value>& a) {
            if (a.empty() || a.size() > 3)
                throw std::runtime_error("range() takes 1 to 3 arguments");
            long long start = 0, stop = 0, step = 1;
            if (a.size() == 1) {
                if (!a[0].isInt()) throw std::runtime_error("range() requires int arguments");
                stop = a[0].asInt();
            }
            else if (a.size() == 2) {
                if (!a[0].isInt() || !a[1].isInt())
                    throw std::runtime_error("range() requires int arguments");
                start = a[0].asInt();
                stop = a[1].asInt();
            }
            else {
                if (!a[0].isInt() || !a[1].isInt() || !a[2].isInt())
                    throw std::runtime_error("range() requires int arguments");
                start = a[0].asInt();
                stop = a[1].asInt();
                step = a[2].asInt();
                if (step == 0) throw std::runtime_error("range() step cannot be 0");
            }
            auto lst = std::make_shared<ListValue>();
            if (step > 0) for (long long i = start; i < stop; i += step) lst->items.emplace_back(i);
            else          for (long long i = start; i > stop; i += step) lst->items.emplace_back(i);
            return Value(lst);
        }

    } // namespace

    void Interpreter::installBuiltins() {
        auto add = [&](const char* name, NativeFnPtr fn) {
            auto c = std::make_shared<Callable>();
            c->kind = Callable::Kind::Native; c->name = name; c->nativeFn = fn;
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
        add("range", bi_range);
    }

} // namespace nova