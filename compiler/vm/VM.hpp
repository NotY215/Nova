#pragma once
#include "bytecode/Chunk.hpp"
#include "interp/Environment.hpp"
#include "interp/Value.hpp"
#include <memory>
#include <stdexcept>
#include <vector>

namespace vayu {

    class VMRuntimeError : public std::runtime_error {
    public:
        int line;
        VMRuntimeError(std::string msg, int l)
            : std::runtime_error(std::move(msg)), line(l) {}
    };

    class VM {
    public:
        explicit VM(std::shared_ptr<Environment> globals);

        /// Run a bytecode chunk.  The chunk must contain a `RETURN_V` as its
        /// last instruction (the compiler emits one automatically).
        void run(std::shared_ptr<Chunk> entryChunk);

    private:
        struct CallFrame {
            std::shared_ptr<Chunk>       chunk;
            size_t                       ip = 0;
            std::shared_ptr<Environment> env;
        };

        std::shared_ptr<Environment> globals_;
        std::vector<Value>           stack_;
        std::vector<CallFrame>       frames_;
        int                          currentLine_ = 0;

        // Accessors for the top frame
        CallFrame& frame() { return frames_.back(); }
        Chunk& chunk() { return *frame().chunk; }
        std::shared_ptr<Environment>& env() { return frame().env; }
        size_t& ip() { return frame().ip; }

        void  push(Value v) { stack_.push_back(std::move(v)); }
        Value pop() { Value v = std::move(stack_.back()); stack_.pop_back(); return v; }
        Value& top() { return stack_.back(); }

        uint8_t readByte();
        int     readU16();
        int     readI16();

        [[noreturn]] void runtimeError(const std::string& msg);

        void doArithmetic(int opcode);
        void doComparison(int opcode);

        void callVMFunction(const std::shared_ptr<Callable>& fn,
            const std::vector<Value>& args);
    };

} // namespace vayu