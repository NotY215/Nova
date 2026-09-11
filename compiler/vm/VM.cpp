#include "VM.hpp"
#include "interp/Interpreter.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace vayu {

    VM::VM(std::shared_ptr<Environment> globals) : globals_(std::move(globals)) {}

    uint8_t VM::readByte() { return chunk().code[ip()++]; }

    int VM::readU16() {
        int v = ((int)chunk().code[ip()] << 8) | (int)chunk().code[ip() + 1];
        ip() += 2;
        return v;
    }

    int VM::readI16() {
        int16_t v = (int16_t)(((uint16_t)chunk().code[ip()] << 8) |
            (uint16_t)chunk().code[ip() + 1]);
        ip() += 2;
        return (int)v;
    }

    [[noreturn]] void VM::runtimeError(const std::string& msg) {
        throw VMRuntimeError(msg, currentLine_);
    }

    // ---------------------------------------------------------------------------

    // Forward declaration — valueEqualsVM is defined further down but used by
    // the IN opcode inside VM::run above its definition.
    static bool valueEqualsVM(const Value& a, const Value& b);

    void VM::run(std::shared_ptr<Chunk> entryChunk) {
        frames_.clear();
        stack_.clear();
        frames_.push_back({ std::move(entryChunk), 0, globals_ });

        while (!frames_.empty()) {
            CallFrame& f = frames_.back();
            if (f.ip >= f.chunk->code.size())
                runtimeError("bytecode ran off the end without RETURN_V");

            currentLine_ = f.chunk->lines[f.ip];
            OpCode op = static_cast<OpCode>(readByte());

            switch (op) {

                // ---- Stack & literals ----
            case OpCode::CONST: {
                int idx = readU16();
                push(chunk().constants[idx]);
                break;
            }
            case OpCode::NONE:    push(Value());      break;
            case OpCode::TRUE_V:  push(Value(true));  break;
            case OpCode::FALSE_V: push(Value(false)); break;
            case OpCode::POP:     (void)pop();        break;
            case OpCode::DUP:     push(top());        break;

                // ---- Variables ----
            case OpCode::LOAD: {
                int idx = readU16();
                const std::string& name = chunk().names[idx];
                Value* slot = env()->lookup(name);
                if (!slot) runtimeError("name '" + name + "' is not defined");
                push(*slot);
                break;
            }
            case OpCode::STORE: {
                int idx = readU16();
                const std::string& name = chunk().names[idx];
                Value v = pop();
                if (!env()->assign(name, v))
                    runtimeError("name '" + name + "' is not defined");
                break;
            }
            case OpCode::DEFINE: {
                int idx = readU16();
                const std::string& name = chunk().names[idx];
                Value v = pop();
                if (!env()->assign(name, v)) env()->define(name, std::move(v));
                break;
            }

                               // ---- Arithmetic ----
            case OpCode::ADD:
            case OpCode::SUB:
            case OpCode::MUL:
            case OpCode::DIV:
            case OpCode::FLOORDIV:
            case OpCode::MOD:
            case OpCode::POW:
                doArithmetic((int)op);
                break;

            case OpCode::NEG: {
                Value v = pop();
                if (v.isInt())        push(Value(-v.asInt()));
                else if (v.isFloat()) push(Value(-v.asFloat()));
                else                  runtimeError("cannot negate " + v.typeName());
                break;
            }

                            // ---- Comparison / logic ----
            case OpCode::EQ: case OpCode::NEQ:
            case OpCode::LT: case OpCode::GT:
            case OpCode::LE: case OpCode::GE:
                doComparison((int)op);
                break;

            case OpCode::NOT: {
                Value v = pop();
                push(Value(!v.truthy()));
                break;
            }

                            // ---- Control flow ----
            case OpCode::JUMP: {
                int off = readI16();
                ip() = (size_t)((int)ip() + off);
                break;
            }
            case OpCode::JUMP_IF_FALSE: {
                int off = readI16();
                Value v = pop();
                if (!v.truthy()) ip() = (size_t)((int)ip() + off);
                break;
            }
            case OpCode::JUMP_IF_TRUE: {
                int off = readI16();
                Value v = pop();
                if (v.truthy()) ip() = (size_t)((int)ip() + off);
                break;
            }

                                     // ---- Iteration ----
            case OpCode::ITER_NEW: {
                Value v = pop();
                if (v.isList()) {
                    push(v);
                    push(Value(0LL));
                    break;
                }
                if (v.isMap()) {
                    // Iterate keys — snapshot into a list.
                    auto keys = std::make_shared<ListValue>();
                    for (auto& [k, _] : v.asMap()->entries)
                        keys->items.push_back(Value(k));
                    push(Value(keys));
                    push(Value(0LL));
                    break;
                }
                if (v.isString()) {
                    auto chars = std::make_shared<ListValue>();
                    for (char c : v.asString())
                        chars->items.push_back(Value(std::string(1, c)));
                    push(Value(chars));
                    push(Value(0LL));
                    break;
                }
                runtimeError("cannot iterate over " + v.typeName());
            }
            case OpCode::ITER_NEXT: {
                int off = readI16();
                size_t idxPos = stack_.size() - 1;
                size_t iterPos = stack_.size() - 2;
                Value iterable = stack_[iterPos];
                long long idx = stack_[idxPos].asInt();
                if (!iterable.isList()) runtimeError("iterator state corrupted");
                auto lst = iterable.asList();
                if (idx < 0 || idx >= (long long)lst->items.size()) {
                    stack_.pop_back();
                    stack_.pop_back();
                    ip() = (size_t)((int)ip() + off);
                    break;
                }
                push(lst->items[(size_t)idx]);
                stack_[idxPos] = Value(idx + 1);
                break;
            }

                                  // ---- Lists ----
            case OpCode::LIST_NEW: {
                int count = readU16();
                auto lst = std::make_shared<ListValue>();
                lst->items.resize((size_t)count);
                for (int i = count - 1; i >= 0; --i)
                    lst->items[(size_t)i] = pop();
                push(Value(lst));
                break;
            }

                                 // ---- Functions ----
            case OpCode::MAKE_FN: {
                int idx = readU16();
                auto& fnChunk = chunk().functions[(size_t)idx];
                auto c = std::make_shared<Callable>();
                c->kind = Callable::Kind::VMFunction;
                c->name = "<fn>";
                c->chunk = fnChunk;
                c->vmParams = fnChunk->paramNames;
                c->closure = env();
                push(Value(c));
                break;
            }

            case OpCode::CALL: {
                int argc = readByte();
                std::vector<Value> args((size_t)argc);
                for (int i = argc - 1; i >= 0; --i)
                    args[(size_t)i] = pop();
                Value callee = pop();

                if (!callee.isCallable())
                    runtimeError("cannot call " + callee.typeName() + " value");

                auto fn = callee.asCallable();
                if (fn->kind == Callable::Kind::VMFunction) {
                    callVMFunction(fn, args);
                }
                else {
                    if (!Interpreter::current_)
                        runtimeError("VM: no interpreter context for builtin call");
                    Value r = Interpreter::current_->callValue(
                        callee, args, SourceLocation{});
                    push(std::move(r));
                }
                break;
            }

            case OpCode::RETURN_V: {
                Value v = pop();
                frames_.pop_back();
                if (frames_.empty()) return;
                push(std::move(v));
                break;
            }

                                 // ---- Structs & classes ----
            case OpCode::NEW_INSTANCE: {
                int nameIdx = readU16();
                int argc = readByte();
                std::string clsName = chunk().names[nameIdx];
                std::vector<Value> args((size_t)argc);
                for (int i = argc - 1; i >= 0; --i) args[(size_t)i] = pop();
                if (!Interpreter::current_)
                    runtimeError("VM: no interpreter context for instance construction");
                Value inst = Interpreter::current_->vmNewInst(
                    clsName, args, SourceLocation{});
                push(std::move(inst));
                break;
            }

            case OpCode::ATTR_GET: {
                int nameIdx = readU16();
                std::string attr = chunk().names[nameIdx];
                Value base = pop();
                if (!Interpreter::current_)
                    runtimeError("VM: no interpreter context for attribute lookup");
                Value v = Interpreter::current_->vmGetAttr(
                    base, attr, SourceLocation{});
                push(std::move(v));
                break;
            }

            case OpCode::ATTR_SET: {
                int nameIdx = readU16();
                std::string attr = chunk().names[nameIdx];
                Value value = pop();
                Value base = pop();
                if (!Interpreter::current_)
                    runtimeError("VM: no interpreter context for attribute write");
                Interpreter::current_->vmSetAttr(base, attr, value, SourceLocation{});
                break;
            }

            case OpCode::SUPER: {
                Value* s = env()->lookup("self");
                Value* k = env()->lookup("__class__");
                if (!s || !k || !s->isInstance() || !k->isClass())
                    runtimeError("super() outside method");
                auto sup = std::make_shared<Callable>();
                sup->kind = Callable::Kind::SuperMethod;
                sup->name = "super";
                sup->boundSelf = s->asInstance();
                sup->superParent = k->asClass()->parent;
                if (!sup->superParent)
                    runtimeError("class '" + k->asClass()->name + "' has no parent");
                push(Value(sup));
                break;
            }
                              // ---- Collections ----
            case OpCode::INDEX_GET: {
                Value idx = pop();
                Value tgt = pop();
                if (tgt.isList()) {
                    if (!idx.isInt())
                        runtimeError("list index must be int, got " + idx.typeName());
                    auto lst = tgt.asList();
                    long long i = idx.asInt();
                    if (i < 0) i += (long long)lst->items.size();
                    if (i < 0 || i >= (long long)lst->items.size())
                        runtimeError("list index out of range");
                    push(lst->items[(size_t)i]);
                    break;
                }
                if (tgt.isMap()) {
                    if (!idx.isString())
                        runtimeError("map key must be str, got " + idx.typeName());
                    auto m = tgt.asMap();
                    auto it = m->entries.find(idx.asString());
                    if (it == m->entries.end())
                        runtimeError("map has no key '" + idx.asString() + "'");
                    push(it->second);
                    break;
                }
                if (tgt.isString()) {
                    if (!idx.isInt())
                        runtimeError("str index must be int, got " + idx.typeName());
                    const std::string& s = tgt.asString();
                    long long i = idx.asInt();
                    if (i < 0) i += (long long)s.size();
                    if (i < 0 || i >= (long long)s.size())
                        runtimeError("string index out of range");
                    push(Value(std::string(1, s[(size_t)i])));
                    break;
                }
                runtimeError("cannot index value of type " + tgt.typeName());
            }

            case OpCode::INDEX_SET: {
                Value v = pop();
                Value idx = pop();
                Value tgt = pop();
                if (tgt.isList()) {
                    if (!idx.isInt())
                        runtimeError("list index must be int, got " + idx.typeName());
                    auto lst = tgt.asList();
                    long long i = idx.asInt();
                    if (i < 0) i += (long long)lst->items.size();
                    if (i < 0 || i >= (long long)lst->items.size())
                        runtimeError("list index out of range");
                    lst->items[(size_t)i] = std::move(v);
                    break;
                }
                if (tgt.isMap()) {
                    if (!idx.isString())
                        runtimeError("map key must be str, got " + idx.typeName());
                    tgt.asMap()->entries[idx.asString()] = std::move(v);
                    break;
                }
                if (tgt.isString())
                    runtimeError("strings are immutable");
                runtimeError("cannot index-assign to value of type " + tgt.typeName());
            }

            case OpCode::MAP_NEW: {
                int count = readU16();
                auto m = std::make_shared<MapValue>();
                std::vector<std::pair<std::string, Value>> pairs((size_t)count);
                for (int i = count - 1; i >= 0; --i) {
                    Value v = pop();
                    Value k = pop();
                    if (!k.isString())
                        runtimeError("map keys must be str, got " + k.typeName());
                    pairs[(size_t)i] = { k.asString(), std::move(v) };
                }
                for (auto& [k, v] : pairs) m->entries[k] = std::move(v);
                push(Value(m));
                break;
            }

            case OpCode::IN: {
                Value r = pop();
                Value l = pop();
                if (r.isList()) {
                    bool found = false;
                    for (auto& v : r.asList()->items)
                        if (valueEqualsVM(l, v)) { found = true; break; }
                    push(Value(found));
                    break;
                }
                if (r.isMap()) {
                    if (!l.isString()) { push(Value(false)); break; }
                    push(Value(r.asMap()->entries.count(l.asString()) > 0));
                    break;
                }
                if (r.isString() && l.isString()) {
                    push(Value(r.asString().find(l.asString()) != std::string::npos));
                    break;
                }
                runtimeError("'in' requires a list, map, or str on the right");
            }

                                 // ---- Misc ----
            case OpCode::PRINT: {
                Value v = pop();
                std::cout << v.toString() << '\n';
                break;
            }
            }
        }
    }

    // ---------------------------------------------------------------------------

    void VM::callVMFunction(const std::shared_ptr<Callable>& fn,
        const std::vector<Value>& args) {
        if (args.size() != fn->vmParams.size())
            runtimeError("function expects " + std::to_string(fn->vmParams.size()) +
                " argument(s), got " + std::to_string(args.size()));

        auto callEnv = std::make_shared<Environment>(
            fn->closure ? fn->closure : globals_);
        for (size_t i = 0; i < args.size(); ++i)
            callEnv->define(fn->vmParams[i], args[i]);

        frames_.push_back({ fn->chunk, 0, callEnv });
    }

    // ---------------------------------------------------------------------------
    // Arithmetic
    // ---------------------------------------------------------------------------

    void VM::doArithmetic(int opcode) {
        Value r = pop();
        Value l = pop();
        OpCode op = static_cast<OpCode>(opcode);

        switch (op) {
        case OpCode::ADD:
            if (l.isInt() && r.isInt()) { push(Value(l.asInt() + r.asInt()));       return; }
            if (l.isNumber() && r.isNumber()) { push(Value(l.asDouble() + r.asDouble())); return; }
            if (l.isString() && r.isString()) { push(Value(l.asString() + r.asString())); return; }
            if (l.isList() && r.isList()) {
                auto out = std::make_shared<ListValue>();
                out->items = l.asList()->items;
                for (auto& v : r.asList()->items) out->items.push_back(v);
                push(Value(out));
                return;
            }
            runtimeError("cannot add " + l.typeName() + " and " + r.typeName());

        case OpCode::SUB:
            if (l.isInt() && r.isInt()) { push(Value(l.asInt() - r.asInt()));       return; }
            if (l.isNumber() && r.isNumber()) { push(Value(l.asDouble() - r.asDouble())); return; }
            runtimeError("cannot subtract " + r.typeName() + " from " + l.typeName());

        case OpCode::MUL: {
            if (l.isInt() && r.isInt()) { push(Value(l.asInt() * r.asInt()));       return; }
            if (l.isNumber() && r.isNumber()) { push(Value(l.asDouble() * r.asDouble())); return; }
            if (l.isString() && r.isInt()) {
                std::string o; for (long long i = 0; i < r.asInt(); ++i) o += l.asString();
                push(Value(std::move(o))); return;
            }
            if (l.isInt() && r.isString()) {
                std::string o; for (long long i = 0; i < l.asInt(); ++i) o += r.asString();
                push(Value(std::move(o))); return;
            }
            if (l.isList() && r.isInt()) {
                auto out = std::make_shared<ListValue>();
                for (long long i = 0; i < r.asInt(); ++i)
                    for (auto& v : l.asList()->items) out->items.push_back(v);
                push(Value(out));
                return;
            }
            if (l.isInt() && r.isList()) {
                auto out = std::make_shared<ListValue>();
                for (long long i = 0; i < l.asInt(); ++i)
                    for (auto& v : r.asList()->items) out->items.push_back(v);
                push(Value(out));
                return;
            }
            runtimeError("cannot multiply " + l.typeName() + " by " + r.typeName());
        }

        case OpCode::DIV: {
            if (l.isNumber() && r.isNumber()) {
                double d = r.asDouble();
                if (d == 0.0) runtimeError("division by zero");
                push(Value(l.asDouble() / d)); return;
            }
            runtimeError("cannot divide " + l.typeName() + " by " + r.typeName());
        }

        case OpCode::FLOORDIV: {
            if (l.isNumber() && r.isNumber()) {
                double d = r.asDouble();
                if (d == 0.0) runtimeError("division by zero");
                double q = std::floor(l.asDouble() / d);
                if (l.isInt() && r.isInt()) push(Value((long long)q));
                else                        push(Value(q));
                return;
            }
            runtimeError("cannot apply '//' to " + l.typeName() + " and " + r.typeName());
        }

        case OpCode::MOD: {
            if (l.isNumber() && r.isNumber()) {
                double d = r.asDouble();
                if (d == 0.0) runtimeError("modulo by zero");
                double m = std::fmod(l.asDouble(), d);
                if (m != 0 && ((m < 0) != (d < 0))) m += d;
                if (l.isInt() && r.isInt()) push(Value((long long)m));
                else                        push(Value(m));
                return;
            }
            runtimeError("cannot apply '%' to " + l.typeName() + " and " + r.typeName());
        }

        case OpCode::POW: {
            if (l.isNumber() && r.isNumber()) {
                if (l.isInt() && r.isInt() && r.asInt() >= 0) {
                    long long base = l.asInt(), exp = r.asInt(), acc = 1;
                    while (exp--) acc *= base;
                    push(Value(acc)); return;
                }
                push(Value(std::pow(l.asDouble(), r.asDouble()))); return;
            }
            runtimeError("cannot apply '**' to " + l.typeName() + " and " + r.typeName());
        }

        default: runtimeError("internal: not an arithmetic op");
        }
    }

    // ---------------------------------------------------------------------------

    static bool valueEqualsVM(const Value& a, const Value& b) {
        if (a.isNumber() && b.isNumber()) return a.asDouble() == b.asDouble();
        if (a.isString() && b.isString()) return a.asString() == b.asString();
        if (a.isBool() && b.isBool())   return a.asBool() == b.asBool();
        if (a.isNone() && b.isNone())   return true;
        if (a.isInstance() && b.isInstance()) return a.asInstance() == b.asInstance();
        if (a.isList() && b.isList()) return a.asList() == b.asList();
        return false;
    }

    void VM::doComparison(int opcode) {
        Value r = pop();
        Value l = pop();
        OpCode op = static_cast<OpCode>(opcode);

        if (op == OpCode::EQ) { push(Value(valueEqualsVM(l, r))); return; }
        if (op == OpCode::NEQ) { push(Value(!valueEqualsVM(l, r))); return; }

        bool res = false;
        if (l.isNumber() && r.isNumber()) {
            double a = l.asDouble(), b = r.asDouble();
            switch (op) {
            case OpCode::LT: res = a < b; break;
            case OpCode::GT: res = a > b; break;
            case OpCode::LE: res = a <= b; break;
            case OpCode::GE: res = a >= b; break;
            default: break;
            }
        }
        else if (l.isString() && r.isString()) {
            const auto& a = l.asString(); const auto& b = r.asString();
            switch (op) {
            case OpCode::LT: res = a < b; break;
            case OpCode::GT: res = a > b; break;
            case OpCode::LE: res = a <= b; break;
            case OpCode::GE: res = a >= b; break;
            default: break;
            }
        }
        else {
            runtimeError("cannot compare " + l.typeName() + " and " + r.typeName());
        }
        push(Value(res));
    }

} // namespace vayu