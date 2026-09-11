#pragma once
#include "Token.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace vayu {

    class Lexer {
    public:
        explicit Lexer(std::string source);
        std::vector<Token> tokenize();

    private:
        std::string source_;
        size_t      pos_ = 0;
        int         line_ = 1;
        int         col_ = 1;

        std::vector<int> indentStack_;
        bool             atLineStart_ = true;
        int              bracketDepth_ = 0;

        std::vector<Token> tokens_;

        bool isAtEnd() const { return pos_ >= source_.size(); }
        char peek(int ahead = 0) const;
        char advance();
        bool match(char expected);
        SourceLocation here() const;
        void add(TokenType type, std::string lexeme = "");
        void addAt(TokenType type, SourceLocation loc, std::string lexeme = "");

        void handleLineStart();
        void skipComment();
        void readIdentifier();
        void readNumber();
        void readString(char quote);
        void readOperator();

        static const std::unordered_map<std::string, TokenType>& keywords();
    };

} // namespace vayu