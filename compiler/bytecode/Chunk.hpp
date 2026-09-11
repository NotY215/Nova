#pragma once
#include "OpCode.hpp"
#include "interp/Value.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace nova {

    /// A compiled compilation unit — top-level script for 4A, per-function later.
    struct Chunk {
        std::vector<uint8_t>     code;
        std::vector<int>         lines;      // parallel to code
        std::vector<Value>       constants;
        std::vector<std::string> names;      // identifiers for LOAD/STORE/DEFINE

        void   emit(uint8_t b, int line);
        void   emitOp(OpCode op, int line);

        int    addConstant(Value v);
        int    addName(const std::string& name);

        size_t here() const { return code.size(); }

        /// Patch a 2-byte signed offset at pos so that VM's (ip+2) + offset lands
        /// on `target`.
        void   patchJump(size_t pos, int target, int line);

        int    readU16(size_t at) const;
        int    readI16(size_t at) const;
    };

    /// Pretty-print for debugging.
    void disassemble(const Chunk& chunk, const char* title);

} // namespace nova