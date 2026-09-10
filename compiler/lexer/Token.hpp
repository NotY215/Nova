#pragma once
#include <string>
#include <cstddef>

namespace nova {

    enum class TokenType {
        // --- Literals ---
        Int, Float, String, Char,
        True, False, None,

        // --- Identifier ---
        Identifier,

        // --- Keywords ---
        Def, Return, If, Elif, Else, While, For, Break, Continue, Pass,
        Class, Struct, Enum, Interface, Self, New, Delete,
        Import, From, As,
        Try, Except, Finally, With, Raise,
        Unsafe, Spawn, Wait, Task, Async, Await,
        And, Or, Not, In, Is, Lambda, Yield,
        // type keywords
        IntKw, FloatKw, BoolKw, StrKw, CharKw, BytesKw,
        Ptr, Ref, Unique, Shared, Weak,

        // --- Operators / punctuation ---
        Plus, Minus, Star, Slash, Percent, StarStar, SlashSlash,
        Assign, Eq, NotEq, Lt, Gt, LtEq, GtEq,
        PlusAssign, MinusAssign, StarAssign, SlashAssign,
        Arrow, FatArrow,
        Dot, Comma, Colon, Semicolon,
        LParen, RParen, LBracket, RBracket, LBrace, RBrace,
        Amp, Pipe, Caret, Tilde, Shl, Shr, At,

        // --- Structural ---
        Newline,        // logical end-of-line
        Indent,         // one level deeper
        Dedent,         // one level shallower
        EndOfFile,
        Invalid,
    };

    struct SourceLocation {
        int    line = 1;
        int    column = 1;
        size_t offset = 0;
    };

    struct Token {
        TokenType     type;
        std::string   lexeme;      // original text
        SourceLocation location;   // start of the token
    };

    /// Human-readable name for diagnostics and the token-dump tool.
    const char* tokenTypeName(TokenType type);

} // namespace nova