#pragma once
#include "Token.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace nova {

    class Lexer {
    public:
        explicit Lexer(std::string source);

        /// Tokenize the whole source. Always ends with EndOfFile.
        std::vector<Token> tokenize();

    private:
        // --- source cursor ---
        std::string source_;
        size_t      pos_ = 0;
        int         line_ = 1;
        int         col_ = 1;

        // --- indentation state ---
        std::vector<int> indentStack_;
        bool             atLineStart_ = true;

        // --- bracket tracking: suppress NEWLINE / INDENT inside () [] {} ---
        int bracketDepth_ = 0;

        // --- output ---
        std::vector<Token> tokens_;

        // --- primitives ---
        bool isAtEnd() const { return pos_ >= source_.size(); }
        char peek(int ahead = 0) const;
        char advance();
        bool match(char expected);
        SourceLocation here() const;
        void add(TokenType type, std::string lexeme = "");

        // --- scanning subroutines ---
        void handleLineStart();
        void skipComment();
        void readIdentifier();
        void readNumber();
        void readString(char quote);
        void readOperator();
        void addAt(TokenType type, SourceLocation loc, std::string lexeme);
        // --- keyword lookup ---
        static const std::unordered_map<std::string, TokenType>& keywords();
    };

} // namespace nova