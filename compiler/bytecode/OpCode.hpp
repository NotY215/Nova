#pragma once
#include <cstdint>

namespace nova {

    /// Nova bytecode instruction set — Phase 4A.
    enum class OpCode : uint8_t {
        // ---- Stack & literals ----
        CONST,          // <index:u16>    push constants[index]
        NONE,           //                push None
        TRUE_V,         //                push true
        FALSE_V,        //                push false
        POP,            //                discard top
        DUP,            //                duplicate top

        // ---- Variables (global scope only in 4A) ----
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

// ---- Calls ----
CALL,               // <argc:u8>      [callee, a0..aN-1] -> result

// ---- Misc ----
PRINT,              // pop, print with newline
RETURN,             // end of script
    };

    const char* opCodeName(OpCode op);

} // namespace nova