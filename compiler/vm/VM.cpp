#include "VM.hpp"
#include "interp/Interpreter.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace nova {

    VM::VM(std::shared_ptr<Environment> globals) : globals_(std::move(globals)) {}

    uint8_t VM::readByte() { return chunk_->code[ip_++]; }

    int VM::readU16() {
        int v = ((int)chunk_->code[ip_] << 8) | (int)chunk_->code[ip_ + 1];
        ip_ += 2;
        return v;
    }

    int VM::readI16() {
        int16_t v = (int16_t)(((uint16_t)chunk_->code[ip_] << 8) |
            (uint16_t)chunk_->code[ip_ + 1]);
        ip_ += 2;
        return (int)v;
    }

    [[noreturn]] void VM::runtimeError(const std::string& msg) {
        throw VMRuntimeError(msg, currentLine_);
    }

    // ---------------------------------------------------------------------------

    void VM::run(const Chunk& chunk) {
        chunk_ = &chunk;
        ip_ = 0;
        stack_.clear();

        for (;;) {
            if (ip_ >= chunk_->code.size())
                runtimeError("bytecode ran off the end without RETURN");
            currentLine_ = chunk_->lines[ip_];

            OpCode op = static_cast<OpCode>(readByte());
            switch (op) {

                // ---- Stack & literals ----
            case OpCode::CONST: {
                int idx = readU16();
                push(chunk_->constants[idx]);
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
                const std::string& name = chunk_->names[idx];
                Value* slot = globals_->lookup(name);
                if (!slot) runtimeError("name '" + name + "' is not defined");
                push(*slot);
                break;
            }
            case OpCode::STORE: {
                int idx = readU16();
                const std::string& name = chunk_->names[idx];
                Value v = pop();
                if (!globals_->assign(name, v))
                    runtimeError("name '" + name + "' is not defined");
                break;
            }
            case OpCode::DEFINE: {
                int idx = readU16();
                const std::string& name = chunk_->names[idx];
                Value v = pop();
                if (!globals_->assign(name, v))
                    globals_->define(name, std::move(v));
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
                ip_ = (size_t)((int)ip_ + off);
                break;
            }
            case OpCode::JUMP_IF_FALSE: {
                int off = readI16();
                Value v = pop();
                if (!v.truthy()) ip_ = (size_t)((int)ip_ + off);
                break;
            }
            case OpCode::JUMP_IF_TRUE: {
                int off = readI16();
                Value v = pop();
                if (v.truthy()) ip_ = (size_t)((int)ip_ + off);
                break;
            }

                                     // ---- Iteration ----
            case OpCode::ITER_NEW: {
                Value v = pop();
                if (!v.isList())
                    runtimeError("cannot iterate over " + v.typeName());
                push(v);
                push(Value(0LL));
                break;
            }
            case OpCode::ITER_NEXT: {
                int off = readI16();
                if (stack_.size() < 2)
                    runtimeError("iterator state corrupted");
                size_t idxPos = stack_.size() - 1;
                size_t iterPos = stack_.size() - 2;
                Value iterable = stack_[iterPos];
                long long idx = stack_[idxPos].asInt();

                if (!iterable.isList())
                    runtimeError("iterator state corrupted");
                auto lst = iterable.asList();
                if (idx < 0 || idx >= (long long)lst->items.size()) {
                    stack_.pop_back();
                    stack_.pop_back();
                    ip_ = (size_t)((int)ip_ + off);
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

                                 // ---- Calls ----
            case OpCode::CALL: {
                int argc = readByte();
                std::vector<Value> args((size_t)argc);
                for (int i = argc - 1; i >= 0; --i)
                    args[(size_t)i] = pop();
                Value callee = pop();
                if (!Interpreter::current_)
                    runtimeError("VM: no interpreter context for builtin call");
                Value result = Interpreter::current_->callValue(
                    callee, args, SourceLocation{});
                push(std::move(result));
                break;
            }

                             // ---- Misc ----
            case OpCode::PRINT: {
                Value v = pop();
                std::cout << v.toString() << '\n';
                break;
            }

            case OpCode::RETURN:
                return;
            }
        }
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
            if (l.isInt() && r.isInt()) { push(Value(l.asInt() + r.asInt()));         return; }
            if (l.isNumber() && r.isNumber()) { push(Value(l.asDouble() + r.asDouble()));   return; }
            if (l.isString() && r.isString()) { push(Value(l.asString() + r.asString()));   return; }
            runtimeError("cannot add " + l.typeName() + " and " + r.typeName());

        case OpCode::SUB:
            if (l.isInt() && r.isInt()) { push(Value(l.asInt() - r.asInt()));         return; }
            if (l.isNumber() && r.isNumber()) { push(Value(l.asDouble() - r.asDouble()));   return; }
            runtimeError("cannot subtract " + r.typeName() + " from " + l.typeName());

        case OpCode::MUL: {
            if (l.isInt() && r.isInt()) { push(Value(l.asInt() * r.asInt()));         return; }
            if (l.isNumber() && r.isNumber()) { push(Value(l.asDouble() * r.asDouble()));   return; }
            if (l.isString() && r.isInt()) {
                std::string o;
                for (long long i = 0; i < r.asInt(); ++i) o += l.asString();
                push(Value(std::move(o)));
                return;
            }
            if (l.isInt() && r.isString()) {
                std::string o;
                for (long long i = 0; i < l.asInt(); ++i) o += r.asString();
                push(Value(std::move(o)));
                return;
            }
            runtimeError("cannot multiply " + l.typeName() + " by " + r.typeName());
        }

        case OpCode::DIV: {
            if (l.isNumber() && r.isNumber()) {
                double d = r.asDouble();
                if (d == 0.0) runtimeError("division by zero");
                push(Value(l.asDouble() / d));
                return;
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
                    push(Value(acc));
                    return;
                }
                push(Value(std::pow(l.asDouble(), r.asDouble())));
                return;
            }
            runtimeError("cannot apply '**' to " + l.typeName() + " and " + r.typeName());
        }

        default: runtimeError("internal: not an arithmetic op");
        }
    }

    // ---------------------------------------------------------------------------
    // Comparison
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

} // namespace nova