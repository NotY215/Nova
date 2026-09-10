#include "Parser.hpp"
#include <cstdlib>

namespace nova {

    static int binPrec(TokenType t) {
        switch (t) {
        case TokenType::Or: return 1;
        case TokenType::And: return 2;
        case TokenType::Eq: case TokenType::NotEq:
        case TokenType::Lt: case TokenType::Gt:
        case TokenType::LtEq: case TokenType::GtEq:
        case TokenType::In: case TokenType::Is: return 4;
        case TokenType::Plus: case TokenType::Minus: return 5;
        case TokenType::Star: case TokenType::Slash:
        case TokenType::SlashSlash: case TokenType::Percent: return 6;
        case TokenType::StarStar: return 8;
        default: return -1;
        }
    }
    static BinOp tokenToBinOp(TokenType t) {
        switch (t) {
        case TokenType::Plus: return BinOp::Add; case TokenType::Minus: return BinOp::Sub;
        case TokenType::Star: return BinOp::Mul; case TokenType::Slash: return BinOp::Div;
        case TokenType::SlashSlash: return BinOp::FloorDiv; case TokenType::Percent: return BinOp::Mod;
        case TokenType::StarStar: return BinOp::Pow; case TokenType::Eq: return BinOp::Eq;
        case TokenType::NotEq: return BinOp::NotEq; case TokenType::Lt: return BinOp::Lt;
        case TokenType::Gt: return BinOp::Gt; case TokenType::LtEq: return BinOp::LtEq;
        case TokenType::GtEq: return BinOp::GtEq; case TokenType::And: return BinOp::And;
        case TokenType::Or: return BinOp::Or; case TokenType::In: return BinOp::In;
        case TokenType::Is: return BinOp::Is; default: return BinOp::Add;
        }
    }

    Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}
    const Token& Parser::peek(int ahead) const {
        size_t i = pos_ + static_cast<size_t>(ahead);
        return i >= tokens_.size() ? tokens_.back() : tokens_[i];
    }
    const Token& Parser::previous() const { return tokens_[pos_ - 1]; }
    bool Parser::isAtEnd() const { return peek().type == TokenType::EndOfFile; }
    bool Parser::check(TokenType t) const { return peek().type == t; }
    bool Parser::match(TokenType t) { if (check(t)) { advance(); return true; } return false; }
    const Token& Parser::advance() { if (!isAtEnd()) ++pos_; return tokens_[pos_ - 1]; }
    const Token& Parser::expect(TokenType t, const char* what) {
        if (check(t)) return advance();
        throw ParseError(std::string("expected ") + what + ", found '" +
            peek().lexeme + "'", peek().location);
    }
    void Parser::skipNewlines() { while (check(TokenType::Newline)) advance(); }

    Block Parser::parseProgram() {
        Block p;
        for (;;) { skipNewlines(); if (isAtEnd()) break; p.stmts.push_back(parseStatement()); }
        return p;
    }
    Block Parser::parseBlock() {
        expect(TokenType::Newline, "newline after ':'");
        expect(TokenType::Indent, "indented block");
        Block b;
        for (;;) {
            skipNewlines();
            if (isAtEnd() || check(TokenType::Dedent)) break;
            b.stmts.push_back(parseStatement());
        }
        match(TokenType::Dedent);
        return b;
    }

    StmtPtr Parser::parseStatement() {
        if (check(TokenType::Def))    return parseDef();
        if (check(TokenType::Struct)) return parseStruct();
        if (check(TokenType::Class))  return parseClass();
        if (check(TokenType::If))     return parseIf();
        if (check(TokenType::While))  return parseWhile();
        if (check(TokenType::Return)) return parseReturn();

        if (check(TokenType::Pass)) {
            Token t = advance();
            return std::make_unique<PassStmt>(t.location);
        }
        if (check(TokenType::Break)) {
            Token t = advance();
            return std::make_unique<BreakStmt>(t.location);
        }
        if (check(TokenType::Continue)) {
            Token t = advance();
            return std::make_unique<ContinueStmt>(t.location);
        }
        if (check(TokenType::Identifier) && peek(1).type == TokenType::Colon)
            return parseAnnotatedAssign();
        return parseExprOrAssign();
    }

    StmtPtr Parser::parseExprOrAssign() {
        SourceLocation start = peek().location;
        ExprPtr expr = parseExpression();
        if (match(TokenType::Assign)) {
            ExprPtr value = parseExpression();
            return std::make_unique<AssignStmt>(std::move(expr), std::move(value), start);
        }
        return std::make_unique<ExprStmt>(std::move(expr), start);
    }
    StmtPtr Parser::parseAnnotatedAssign() {
        Token name = advance(); advance();
        ExprPtr type = parseExpression();
        ExprPtr value = nullptr;
        if (match(TokenType::Assign)) value = parseExpression();
        return std::make_unique<AnnotAssignStmt>(name.lexeme, std::move(type),
            std::move(value), name.location);
    }
    StmtPtr Parser::parseReturn() {
        Token t = advance();
        ExprPtr value = nullptr;
        if (!check(TokenType::Newline) && !check(TokenType::Dedent) && !isAtEnd())
            value = parseExpression();
        return std::make_unique<ReturnStmt>(std::move(value), t.location);
    }
    StmtPtr Parser::parseIf() {
        Token ifTok = advance();
        ExprPtr cond = parseExpression();
        expect(TokenType::Colon, "':' after if condition");
        Block thenBody = parseBlock();
        auto stmt = std::make_unique<IfStmt>(std::move(cond), std::move(thenBody), ifTok.location);
        while (check(TokenType::Elif)) {
            advance();
            ExprPtr econd = parseExpression();
            expect(TokenType::Colon, "':' after elif condition");
            Block ebody = parseBlock();
            stmt->elifs.push_back(ElifClause{ std::move(econd), std::move(ebody) });
        }
        if (check(TokenType::Else)) {
            advance();
            expect(TokenType::Colon, "':' after else");
            stmt->elseBody = parseBlock();
        }
        return stmt;
    }
    StmtPtr Parser::parseWhile() {
        Token wTok = advance();
        ExprPtr cond = parseExpression();
        expect(TokenType::Colon, "':' after while condition");
        Block body = parseBlock();
        return std::make_unique<WhileStmt>(std::move(cond), std::move(body), wTok.location);
    }
    Param Parser::parseParam() {
        Token name = expect(TokenType::Identifier, "parameter name");
        Param p; p.name = name.lexeme;
        if (match(TokenType::Colon)) p.type = parseExpression();
        return p;
    }
    std::unique_ptr<DefStmt> Parser::parseDef() {
        Token defTok = advance();
        Token name = expect(TokenType::Identifier, "function name");
        expect(TokenType::LParen, "'(' after function name");
        std::vector<Param> params;
        if (!check(TokenType::RParen)) {
            params.push_back(parseParam());
            while (match(TokenType::Comma)) {
                if (check(TokenType::RParen)) break;
                params.push_back(parseParam());
            }
        }
        expect(TokenType::RParen, "')' to close parameter list");
        ExprPtr retType = nullptr;
        if (match(TokenType::Arrow)) retType = parseExpression();
        expect(TokenType::Colon, "':' before function body");
        Block body = parseBlock();
        return std::make_unique<DefStmt>(name.lexeme, std::move(params),
            std::move(retType), std::move(body),
            defTok.location);
    }

    FieldDef Parser::parseFieldDef() {
        Token name = expect(TokenType::Identifier, "field name");
        expect(TokenType::Colon, "':' after field name");
        ExprPtr type = parseExpression();
        FieldDef f; f.name = name.lexeme; f.type = std::move(type); f.loc = name.location;
        if (!check(TokenType::Newline) && !check(TokenType::Dedent) && !isAtEnd())
            throw ParseError("unexpected token after field type", peek().location);
        return f;
    }

    StmtPtr Parser::parseStruct() {
        Token stTok = advance();
        Token name = expect(TokenType::Identifier, "struct name");
        expect(TokenType::Colon, "':' after struct name");
        expect(TokenType::Newline, "newline after ':'");
        expect(TokenType::Indent, "indented struct body");
        std::vector<FieldDef> fields;
        for (;;) {
            skipNewlines();
            if (isAtEnd() || check(TokenType::Dedent)) break;
            fields.push_back(parseFieldDef());
        }
        match(TokenType::Dedent);
        if (fields.empty())
            throw ParseError("struct '" + name.lexeme + "' has no fields", stTok.location);
        return std::make_unique<StructStmt>(name.lexeme, std::move(fields), stTok.location);
    }

    StmtPtr Parser::parseClass() {
        Token cTok = advance();
        Token name = expect(TokenType::Identifier, "class name");

        std::string parentName;
        if (match(TokenType::LParen)) {
            Token p = expect(TokenType::Identifier, "parent class name");
            parentName = p.lexeme;
            expect(TokenType::RParen, "')' after parent class");
        }
        expect(TokenType::Colon, "':' after class name");
        expect(TokenType::Newline, "newline after ':'");
        expect(TokenType::Indent, "indented class body");

        auto cls = std::make_unique<ClassStmt>(name.lexeme, parentName, cTok.location);

        for (;;) {
            skipNewlines();
            if (isAtEnd() || check(TokenType::Dedent)) break;
            if (check(TokenType::Def)) {
                auto m = parseDef();
                // Enforce `self` as first param
                if (m->params.empty() || m->params[0].name != "self")
                    throw ParseError("method '" + m->name +
                        "' must have 'self' as its first parameter",
                        m->loc);
                cls->methods.push_back(std::move(m));
            }
            else if (check(TokenType::Identifier) && peek(1).type == TokenType::Colon) {
                cls->fields.push_back(parseFieldDef());
            }
            else {
                throw ParseError("expected field or method in class body",
                    peek().location);
            }
        }
        match(TokenType::Dedent);

        if (cls->fields.empty() && cls->methods.empty())
            throw ParseError("class '" + name.lexeme + "' is empty", cTok.location);

        // Detect field/method collisions
        for (auto& f : cls->fields)
            for (auto& m : cls->methods)
                if (f.name == m->name)
                    throw ParseError("'" + f.name +
                        "' declared as both field and method", f.loc);

        return cls;
    }

    ExprPtr Parser::parseExpression() { return parseBinary(1); }
    ExprPtr Parser::parseBinary(int minPrec) {
        ExprPtr left = parseUnary();
        for (;;) {
            int prec = binPrec(peek().type);
            if (prec < minPrec) break;
            Token opTok = advance();
            BinOp op = tokenToBinOp(opTok.type);
            int nextMin = (op == BinOp::Pow) ? prec : prec + 1;
            ExprPtr right = parseBinary(nextMin);
            left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), opTok.location);
        }
        return left;
    }
    ExprPtr Parser::parseUnary() {
        if (match(TokenType::Minus)) {
            Token t = previous();
            return std::make_unique<UnaryExpr>(UnOp::Neg, parseUnary(), t.location);
        }
        if (match(TokenType::Plus)) {
            Token t = previous();
            return std::make_unique<UnaryExpr>(UnOp::Pos, parseUnary(), t.location);
        }
        if (match(TokenType::Not)) {
            Token t = previous();
            return std::make_unique<UnaryExpr>(UnOp::Not, parseUnary(), t.location);
        }
        return parsePostfix();
    }
    CallArg Parser::parseCallArg() {
        CallArg a; a.loc = peek().location;
        if (check(TokenType::Identifier) && peek(1).type == TokenType::Assign) {
            a.name = advance().lexeme;
            advance();
            a.value = parseExpression();
            return a;
        }
        a.value = parseExpression();
        return a;
    }
    ExprPtr Parser::parsePostfix() {
        ExprPtr e = parsePrimary();
        for (;;) {
            if (match(TokenType::Dot)) {
                Token name = expect(TokenType::Identifier, "attribute name after '.'");
                e = std::make_unique<AttrExpr>(std::move(e), name.lexeme, name.location);
            }
            else if (check(TokenType::LParen)) {
                Token open = advance();
                std::vector<CallArg> args;
                if (!check(TokenType::RParen)) {
                    args.push_back(parseCallArg());
                    while (match(TokenType::Comma)) {
                        if (check(TokenType::RParen)) break;
                        args.push_back(parseCallArg());
                    }
                }
                expect(TokenType::RParen, "')' to close argument list");
                e = std::make_unique<CallExpr>(std::move(e), std::move(args), open.location);
            }
            else if (check(TokenType::LBracket)) {
                Token open = advance();
                ExprPtr idx = parseExpression();
                expect(TokenType::RBracket, "']' to close index");
                e = std::make_unique<IndexExpr>(std::move(e), std::move(idx), open.location);
            }
            else break;
        }
        return e;
    }
    ExprPtr Parser::parsePrimary() {
        const Token& t = peek();
        switch (t.type) {
        case TokenType::Int: {
            Token tok = advance();
            long long v = 0;
            try { v = std::stoll(tok.lexeme); }
            catch (...) { throw ParseError("integer literal out of range", tok.location); }
            return std::make_unique<IntLitExpr>(v, tok.lexeme, tok.location);
        }
        case TokenType::Float: {
            Token tok = advance();
            double v = 0;
            try { v = std::stod(tok.lexeme); }
            catch (...) { throw ParseError("bad float literal", tok.location); }
            return std::make_unique<FloatLitExpr>(v, tok.lexeme, tok.location);
        }
        case TokenType::String: {
            Token tok = advance();
            return std::make_unique<StringLitExpr>(tok.lexeme, tok.location);
        }
        case TokenType::Char: {
            Token tok = advance();
            return std::make_unique<CharLitExpr>(tok.lexeme, tok.location);
        }
        case TokenType::True: {
            Token tok = advance();
            return std::make_unique<BoolLitExpr>(true, tok.location);
        }
        case TokenType::False: {
            Token tok = advance();
            return std::make_unique<BoolLitExpr>(false, tok.location);
        }
        case TokenType::None: {
            Token tok = advance();
            return std::make_unique<NoneLitExpr>(tok.location);
        }
        case TokenType::Identifier:
        case TokenType::IntKw: case TokenType::FloatKw: case TokenType::BoolKw:
        case TokenType::StrKw: case TokenType::CharKw: case TokenType::BytesKw:
        case TokenType::Ptr: case TokenType::Ref: case TokenType::Unique:
        case TokenType::Shared: case TokenType::Weak: {
            Token tok = advance();
            return std::make_unique<NameRefExpr>(tok.lexeme, tok.location);
        }
        case TokenType::LParen: {
            Token open = advance();
            ExprPtr inner = parseExpression();
            expect(TokenType::RParen, "')' to close group");
            return std::make_unique<GroupingExpr>(std::move(inner), open.location);
        }
        default:
            throw ParseError(std::string("expected expression, found '") +
                t.lexeme + "'", t.location);
        }
    }

} // namespace nova