#include "Parser.hpp"
#include <cstdlib>

namespace vayu {

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
        if (check(TokenType::For))    return parseFor();
        if (check(TokenType::Return)) return parseReturn();
        if (check(TokenType::Try))    return parseTry();
        if (check(TokenType::Raise))  return parseRaise();
        if (check(TokenType::Import)) return parseImport();
        if (check(TokenType::From))   return parseFromImport();

        if (check(TokenType::Pass)) { Token t = advance(); return std::make_unique<PassStmt>(t.location); }
        if (check(TokenType::Break)) { Token t = advance(); return std::make_unique<BreakStmt>(t.location); }
        if (check(TokenType::Continue)) { Token t = advance(); return std::make_unique<ContinueStmt>(t.location); }

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
        ExprPtr type = parseTypeExpr();
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
    StmtPtr Parser::parseRaise() {
        Token t = advance();
        ExprPtr exc = nullptr;
        if (!check(TokenType::Newline) && !check(TokenType::Dedent) && !isAtEnd())
            exc = parseExpression();
        return std::make_unique<RaiseStmt>(std::move(exc), t.location);
    }
    StmtPtr Parser::parseTry() {
        Token tryTok = advance();
        expect(TokenType::Colon, "':' after try");
        Block tryBody = parseBlock();
        auto stmt = std::make_unique<TryStmt>(std::move(tryBody), tryTok.location);
        bool sawExcept = false;
        while (check(TokenType::Except)) {
            sawExcept = true;
            advance();
            ExceptClause clause;
            if (!check(TokenType::Colon)) {
                clause.exceptionType = parseExpression();
                if (match(TokenType::As)) {
                    Token var = expect(TokenType::Identifier, "variable name after 'as'");
                    clause.varName = var.lexeme;
                }
            }
            expect(TokenType::Colon, "':' after except");
            clause.body = parseBlock();
            stmt->handlers.push_back(std::move(clause));
        }
        if (match(TokenType::Finally)) {
            expect(TokenType::Colon, "':' after finally");
            stmt->finallyBody = parseBlock();
        }
        if (!sawExcept && !stmt->finallyBody)
            throw ParseError("try block must have at least one except or a finally",
                tryTok.location);
        return stmt;
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
    StmtPtr Parser::parseFor() {
        Token fTok = advance();
        Token varName = expect(TokenType::Identifier, "loop variable name");
        expect(TokenType::In, "'in' after loop variable");
        ExprPtr iterable = parseExpression();
        expect(TokenType::Colon, "':' after iterable");
        Block body = parseBlock();
        return std::make_unique<ForStmt>(varName.lexeme, std::move(iterable),
            std::move(body), fTok.location);
    }
    Param Parser::parseParam() {
        Token name = expect(TokenType::Identifier, "parameter name");
        Param p; p.name = name.lexeme;
        if (match(TokenType::Colon)) p.type = parseTypeExpr();
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
        if (match(TokenType::Arrow)) retType = parseTypeExpr();
        expect(TokenType::Colon, "':' before function body");
        Block body = parseBlock();
        return std::make_unique<DefStmt>(name.lexeme, std::move(params),
            std::move(retType), std::move(body),
            defTok.location);
    }

    StmtPtr Parser::parseImport() {
        Token imp = advance();   // 'import'
        Token name = expect(TokenType::Identifier, "module name");
        std::string alias;
        if (match(TokenType::As)) {
            Token a = expect(TokenType::Identifier, "alias after 'as'");
            alias = a.lexeme;
        }
        return std::make_unique<ImportStmt>(name.lexeme, alias, imp.location);
    }

    StmtPtr Parser::parseFromImport() {
        Token fromTok = advance();   // 'from'
        Token module = expect(TokenType::Identifier, "module name");
        expect(TokenType::Import, "'import' after module name");

        std::vector<ImportItem> items;
        for (;;) {
            ImportItem it;
            Token n = expect(TokenType::Identifier, "name to import");
            it.name = n.lexeme;
            if (match(TokenType::As)) {
                Token a = expect(TokenType::Identifier, "alias after 'as'");
                it.alias = a.lexeme;
            }
            items.push_back(std::move(it));
            if (!match(TokenType::Comma)) break;
        }
        return std::make_unique<FromImportStmt>(module.lexeme, std::move(items), fromTok.location);
    }

    FieldDef Parser::parseFieldDef() {
        Token name = expect(TokenType::Identifier, "field name");
        expect(TokenType::Colon, "':' after field name");
        ExprPtr type = parseTypeExpr();
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
                if (m->params.empty() || m->params[0].name != "self")
                    throw ParseError("method '" + m->name +
                        "' must have 'self' as its first parameter", m->loc);
                cls->methods.push_back(std::move(m));
            }
            else if (check(TokenType::Identifier) && peek(1).type == TokenType::Colon) {
                cls->fields.push_back(parseFieldDef());
            }
            else throw ParseError("expected field or method in class body", peek().location);
        }
        match(TokenType::Dedent);
        if (cls->fields.empty() && cls->methods.empty())
            throw ParseError("class '" + name.lexeme + "' is empty", cTok.location);
        for (auto& f : cls->fields)
            for (auto& m : cls->methods)
                if (f.name == m->name)
                    throw ParseError("'" + f.name +
                        "' declared as both field and method", f.loc);
        return cls;
    }

    static bool isTypeStart(TokenType t) {
        switch (t) {
        case TokenType::Identifier:
        case TokenType::IntKw:   case TokenType::FloatKw: case TokenType::BoolKw:
        case TokenType::StrKw:   case TokenType::CharKw:  case TokenType::BytesKw:
        case TokenType::Ptr:     case TokenType::Ref:
        case TokenType::Unique:  case TokenType::Shared:  case TokenType::Weak:
            return true;
        default: return false;
        }
    }
    static bool isPrimitiveTypeName(TokenType t) {
        switch (t) {
        case TokenType::IntKw: case TokenType::FloatKw: case TokenType::BoolKw:
        case TokenType::StrKw: case TokenType::CharKw:  case TokenType::BytesKw:
        case TokenType::Ptr:   case TokenType::Ref:
        case TokenType::Unique:case TokenType::Shared:  case TokenType::Weak:
            return true;
        default: return false;
        }
    }
    ExprPtr Parser::parseTypeExpr() {
        const Token& t = peek();
        if (!isTypeStart(t.type))
            throw ParseError(std::string("expected type name, found '") + t.lexeme + "'",
                t.location);
        Token name = advance();
        if (isPrimitiveTypeName(name.type) || !check(TokenType::Lt))
            return std::make_unique<NameRefExpr>(name.lexeme, name.location);
        advance();
        auto node = std::make_unique<GenericTypeExpr>(name.lexeme, name.location);
        node->typeArgs.push_back(parseTypeExpr());
        while (match(TokenType::Comma))
            node->typeArgs.push_back(parseTypeExpr());
        expect(TokenType::Gt, "'>' to close type argument list");
        return node;
    }

    // ---- expression parsing ----
    ExprPtr Parser::parseExpression() {
        if (check(TokenType::Lambda)) return parseLambda();
        return parseBinary(1);
    }

    ExprPtr Parser::parseLambda() {
        Token lamTok = advance();   // 'lambda'
        auto node = std::make_unique<LambdaExpr>(lamTok.location);
        if (!check(TokenType::Colon)) {
            Token p = expect(TokenType::Identifier, "lambda parameter name");
            node->params.push_back(p.lexeme);
            while (match(TokenType::Comma)) {
                p = expect(TokenType::Identifier, "lambda parameter name");
                node->params.push_back(p.lexeme);
            }
        }
        expect(TokenType::Colon, "':' after lambda parameters");
        node->body = parseExpression();
        return node;
    }

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
    ExprPtr Parser::parseListLit() {
        Token open = advance();
        auto node = std::make_unique<ListLitExpr>(open.location);
        if (!check(TokenType::RBracket)) {
            node->elements.push_back(parseExpression());
            while (match(TokenType::Comma)) {
                if (check(TokenType::RBracket)) break;
                node->elements.push_back(parseExpression());
            }
        }
        expect(TokenType::RBracket, "']' to close list literal");
        return node;
    }
    ExprPtr Parser::parseMapLit() {
        Token open = advance();
        auto node = std::make_unique<MapLitExpr>(open.location);
        if (!check(TokenType::RBrace)) {
            ExprPtr k = parseExpression();
            expect(TokenType::Colon, "':' between key and value");
            ExprPtr v = parseExpression();
            node->entries.push_back({ std::move(k), std::move(v) });
            while (match(TokenType::Comma)) {
                if (check(TokenType::RBrace)) break;
                k = parseExpression();
                expect(TokenType::Colon, "':' between key and value");
                v = parseExpression();
                node->entries.push_back({ std::move(k), std::move(v) });
            }
        }
        expect(TokenType::RBrace, "'}' to close map literal");
        return node;
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
        case TokenType::LBracket: return parseListLit();
        case TokenType::LBrace:   return parseMapLit();
        default:
            throw ParseError(std::string("expected expression, found '") +
                t.lexeme + "'", t.location);
        }
    }

} // namespace vayu