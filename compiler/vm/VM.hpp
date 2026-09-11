#pragma once
#include "bytecode/Chunk.hpp"
#include "interp/Environment.hpp"
#include "interp/Value.hpp"
#include <memory>
#include <stdexcept>
#include <vector>

namespace nova {

    class VMRuntimeError : public std::runtime_error {
    public:
        int line;
        VMRuntimeError(std::string msg, int l)
            : std::runtime_error(std::move(msg)), line(l) {}
    };

    class VM {
    public:
        explicit VM(std::shared_ptr<Environment> globals);

        void run(const Chunk& chunk);

    private:
        std::shared_ptr<Environment> globals_;
        std::vector<Value>           stack_;
        const Chunk* chunk_ = nullptr;
        size_t                       ip_ = 0;
        int                          currentLine_ = 0;

        void  push(Value v) { stack_.push_back(std::move(v)); }
        Value pop() { Value v = std::move(stack_.back()); stack_.pop_back(); return v; }
        Value& top() { return stack_.back(); }

        uint8_t readByte();
        int     readU16();
        int     readI16();

        [[noreturn]] void runtimeError(const std::string& msg);

        void doArithmetic(int opcode);
        void doComparison(int opcode);
    };

} // namespace nova