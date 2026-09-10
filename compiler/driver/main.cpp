#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "ast/Ast.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void usage() {
    std::fprintf(stderr,
        "usage: novac <file.nova> [--dump-tokens | --dump-ast]\n");
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 1; }

    std::string file = argv[1];
    bool dumpTokens = false;
    bool dumpAst = false;

    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--dump-tokens")) dumpTokens = true;
        else if (!std::strcmp(argv[i], "--dump-ast"))    dumpAst = true;
        else { std::fprintf(stderr, "novac: unknown flag '%s'\n", argv[i]); return 1; }
    }
    if (!dumpTokens && !dumpAst) dumpAst = true;   // default now AST

    std::string src = readFile(file);
    if (src.empty()) {
        std::fprintf(stderr, "novac: cannot read '%s'\n", file.c_str());
        return 1;
    }

    nova::Lexer lexer(std::move(src));
    auto tokens = lexer.tokenize();

    if (dumpTokens) {
        for (const auto& t : tokens) {
            std::printf("%3d:%-3d  %-14s  %s\n",
                t.location.line, t.location.column,
                nova::tokenTypeName(t.type),
                t.lexeme.c_str());
        }
        return 0;
    }

    try {
        nova::Parser parser(std::move(tokens));
        nova::Block program = parser.parseProgram();
        std::printf("Program (%zu statements):\n", program.stmts.size());
        nova::printBlock(program, 1);
    }
    catch (const nova::ParseError& e) {
        std::fprintf(stderr, "%s:%d:%d: error: %s\n",
            file.c_str(), e.loc.line, e.loc.column, e.what());
        return 1;
    }
    return 0;
}