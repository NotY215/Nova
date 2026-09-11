#include "Chunk.hpp"
#include <cstdio>

namespace nova {

    void Chunk::emit(uint8_t b, int line) {
        code.push_back(b);
        lines.push_back(line);
    }

    void Chunk::emitOp(OpCode op, int line) {
        emit(static_cast<uint8_t>(op), line);
    }

    int Chunk::addConstant(Value v) {
        for (size_t i = 0; i < constants.size(); ++i) {
            const Value& c = constants[i];
            if (c.isInt() && v.isInt() && c.asInt() == v.asInt())    return (int)i;
            if (c.isFloat() && v.isFloat() && c.asFloat() == v.asFloat())  return (int)i;
            if (c.isString() && v.isString() && c.asString() == v.asString()) return (int)i;
        }
        constants.push_back(std::move(v));
        return (int)constants.size() - 1;
    }

    int Chunk::addName(const std::string& name) {
        for (size_t i = 0; i < names.size(); ++i)
            if (names[i] == name) return (int)i;
        names.push_back(name);
        return (int)names.size() - 1;
    }

    void Chunk::patchJump(size_t pos, int target, int /*line*/) {
        int offset = target - (int)(pos + 2);
        code[pos] = (uint8_t)((offset >> 8) & 0xFF);
        code[pos + 1] = (uint8_t)(offset & 0xFF);
    }

    int Chunk::readU16(size_t at) const {
        return ((int)code[at] << 8) | (int)code[at + 1];
    }

    int Chunk::readI16(size_t at) const {
        int16_t v = (int16_t)(((uint16_t)code[at] << 8) | (uint16_t)code[at + 1]);
        return (int)v;
    }

    // ---------------------------------------------------------------------------

    static void printConst(const Value& v) {
        if (v.isString()) std::printf("\"%s\"", v.asString().c_str());
        else              std::printf("%s", v.toString().c_str());
    }

    void disassemble(const Chunk& chunk, const char* title) {
        std::printf("=== %s (%zu bytes, %zu consts, %zu names) ===\n",
            title, chunk.code.size(), chunk.constants.size(),
            chunk.names.size());

        size_t i = 0;
        while (i < chunk.code.size()) {
            std::printf("%04zu  L%-3d  ", i, chunk.lines[i]);
            OpCode op = static_cast<OpCode>(chunk.code[i]);
            ++i;
            std::printf("%-16s", opCodeName(op));

            switch (op) {
            case OpCode::CONST: {
                int idx = chunk.readU16(i); i += 2;
                std::printf(" %d (", idx); printConst(chunk.constants[idx]);
                std::printf(")");
                break;
            }
            case OpCode::LOAD:
            case OpCode::STORE:
            case OpCode::DEFINE: {
                int idx = chunk.readU16(i); i += 2;
                std::printf(" %d (%s)", idx, chunk.names[idx].c_str());
                break;
            }
            case OpCode::JUMP:
            case OpCode::JUMP_IF_FALSE:
            case OpCode::JUMP_IF_TRUE:
            case OpCode::ITER_NEXT: {
                int off = chunk.readI16(i); i += 2;
                std::printf(" -> %04zu", (size_t)((int)i + off));
                break;
            }
            case OpCode::LIST_NEW: {
                int cnt = chunk.readU16(i); i += 2;
                std::printf(" %d", cnt);
                break;
            }
            case OpCode::CALL: {
                std::printf(" %d", chunk.code[i]); ++i;
                break;
            }
            default: break;
            }
            std::printf("\n");
        }
        std::printf("\n");
    }

} // namespace nova