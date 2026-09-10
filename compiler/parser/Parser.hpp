#pragma once
#include "ast/Ast.hpp"
#include "lexer/Token.hpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace nova {

    class ParseError : public std::runtime_error {
    public:
        SourceLocation loc;
        ParseError(std::string msg, SourceLocation l)
            : std::runtime_error(std::move(msg)), loc(l) {}
    };

    class Parser {
    public:
        explicit Parser(std::vector<Token> tokens);

        /// Parse the whole file into a program block.
        Block parseProgram();

    private:
        std::vector<Token> tokens_;
        size_t             pos_ = 0;

        // --- cursor ---
        const Token& peek(int ahead = 0) const;
        const Token& previous() const;
        bool         isAtEnd() const;
        bool         check(TokenType t) const;
        bool         match(TokenType t);
        const Token& advance();
        const Token& expect(TokenType t, const char* what);
        void         skipNewlines();

        // --- statements ---
        StmtPtr  parseStatement();
        Block    parseBlock();
        StmtPtr  parseDef();
        StmtPtr  parseIf();
        StmtPtr  parseWhile();
        StmtPtr  parseReturn();
        StmtPtr  parseAnnotatedAssign();
        StmtPtr  parseExprOrAssign();
        Param    parseParam();

        // --- expression grammar (unchanged from Step 2) ---
        ExprPtr  parseExpression();
        ExprPtr  parseBinary(int minPrec);
        ExprPtr  parseUnary();
        ExprPtr  parsePostfix();
        ExprPtr  parsePrimary();
    };

} // namespace nova