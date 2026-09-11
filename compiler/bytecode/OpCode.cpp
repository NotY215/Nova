#include "OpCode.hpp"

namespace vayu {

    const char* opCodeName(OpCode op) {
        switch (op) {
        case OpCode::CONST:    return "CONST";
        case OpCode::NONE:     return "NONE";
        case OpCode::TRUE_V:   return "TRUE";
        case OpCode::FALSE_V:  return "FALSE";
        case OpCode::POP:      return "POP";
        case OpCode::DUP:      return "DUP";
        case OpCode::LOAD:     return "LOAD";
        case OpCode::STORE:    return "STORE";
        case OpCode::DEFINE:   return "DEFINE";
        case OpCode::ADD:      return "ADD";
        case OpCode::SUB:      return "SUB";
        case OpCode::MUL:      return "MUL";
        case OpCode::DIV:      return "DIV";
        case OpCode::FLOORDIV: return "FLOORDIV";
        case OpCode::MOD:      return "MOD";
        case OpCode::POW:      return "POW";
        case OpCode::NEG:      return "NEG";
        case OpCode::EQ:       return "EQ";
        case OpCode::NEQ:      return "NEQ";
        case OpCode::LT:       return "LT";
        case OpCode::GT:       return "GT";
        case OpCode::LE:       return "LE";
        case OpCode::GE:       return "GE";
        case OpCode::NOT:      return "NOT";
        case OpCode::JUMP:           return "JUMP";
        case OpCode::NEW_INSTANCE: return "NEW_INSTANCE";
        case OpCode::ATTR_GET:     return "ATTR_GET";
        case OpCode::ATTR_SET:     return "ATTR_SET";
        case OpCode::SUPER:        return "SUPER";
        case OpCode::JUMP_IF_FALSE:  return "JUMP_IF_FALSE";
        case OpCode::JUMP_IF_TRUE:   return "JUMP_IF_TRUE";
        case OpCode::ITER_NEW:       return "ITER_NEW";
        case OpCode::ITER_NEXT:      return "ITER_NEXT";
        case OpCode::LIST_NEW:       return "LIST_NEW";
        case OpCode::MAKE_FN:        return "MAKE_FN";
        case OpCode::CALL:           return "CALL";
        case OpCode::RETURN_V:       return "RETURN_V";
        case OpCode::PRINT:          return "PRINT";
        }
        return "?";
    }

} // namespace vayu