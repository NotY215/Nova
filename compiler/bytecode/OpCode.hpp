#pragma once
#include <cstdint>

namespace vayu {

    /// vayu bytecode instruction set — Phase 4B.
    enum class OpCode : uint8_t {
        // ---- Stack & literals ----
        CONST,          // <index:u16>    push constants[index]
        NONE,           //                push None
        TRUE_V,         //                push true
        FALSE_V,        //                push false
        POP,            //                discard top
        DUP,            //                duplicate top

        // ---- Variables ----
        LOAD,           // <name:u16>     push value of name
        STORE,          // <name:u16>     pop -> assign existing name
        DEFINE,         // <name:u16>     pop -> define-or-reassign name

        // ---- Arithmetic ----
        ADD, SUB, MUL, DIV, FLOORDIV, MOD, POW,
        NEG,            //                unary minus

        // ---- Comparison / logic ----
        EQ, NEQ, LT, GT, LE, GE,
        NOT,

        // ---- Control flow ----
        JUMP,               // <offset:i16>
        JUMP_IF_FALSE,      // <offset:i16>   pop; jump if falsy
        JUMP_IF_TRUE,       // <offset:i16>   pop; jump if truthy

        // ---- Iteration ----
        ITER_NEW,           // pop iterable; push [iterable, idx=0]
        ITER_NEXT,          // <offset:i16>   peek state; if done: pop 2, jump.
        //                else: push next elem, ++idx

// ---- Lists ----
LIST_NEW,           // <count:u16>    pop count elems, push list

// ---- Structs & classes (Phase 4C) ----
NEW_INSTANCE,       // <nameIdx:u16> <argc:u8>   pop argc, push instance
ATTR_GET,           // <nameIdx:u16>              pop base, push attr
ATTR_SET,           // <nameIdx:u16>              pop value, pop base
SUPER,              // (no operands)              push super proxy

// ---- Functions ----
MAKE_FN,            // <idx:u16>      push Callable for functions[idx],
//                closure = current env
CALL,               // <argc:u8>      [callee, a0..aN-1] -> result
RETURN_V,           //                pop value; pop frame; push to caller

// ---- Misc ----
PRINT,              // pop, print with newline
    };

    const char* opCodeName(OpCode op);

} // namespace vayu