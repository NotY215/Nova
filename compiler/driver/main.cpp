#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "ast/Ast.hpp"
#include "interp/Interpreter.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void usage() {
    std::fprintf(stderr,
        "usage: novac <file.nova> [--run | --dump-tokens | --dump-ast]\n"
        "       --run          execute the program (default)\n"
        "       --dump-tokens  print the lexer output\n"
        "       --dump-ast     print the parsed AST\n");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc < 2) { usage(); return 1; }

    std::string file = argv[1];
    enum class Mode { Run, DumpTokens, DumpAst } mode = Mode::Run;

    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--run"))         mode = Mode::Run;
        else if (!std::strcmp(argv[i], "--dump-tokens")) mode = Mode::DumpTokens;
        else if (!std::strcmp(argv[i], "--dump-ast"))    mode = Mode::DumpAst;
        else { std::fprintf(stderr, "novac: unknown flag '%s'\n", argv[i]); return 1; }
    }

    std::string src = readFile(file);
    if (src.empty()) {
        std::fprintf(stderr, "novac: cannot read '%s'\n", file.c_str());
        return 1;
    }

    nova::Lexer lexer(std::move(src));
    auto tokens = lexer.tokenize();

    if (mode == Mode::DumpTokens) {
        for (const auto& t : tokens) {
            std::printf("%3d:%-3d  %-14s  %s\n",
                t.location.line, t.location.column,
                nova::tokenTypeName(t.type),
                t.lexeme.c_str());
        }
        return 0;
    }

    nova::Block program;
    try {
        nova::Parser parser(std::move(tokens));
        program = parser.parseProgram();
    }
    catch (const nova::ParseError& e) {
        std::fprintf(stderr, "%s:%d:%d: error: %s\n",
            file.c_str(), e.loc.line, e.loc.column, e.what());
        return 1;
    }

    if (mode == Mode::DumpAst) {
        nova::printProgram(program);
        return 0;
    }

    // Run mode
    try {
        nova::Interpreter interp;
        interp.run(program);
    }
    catch (const nova::RuntimeError& e) {
        std::fprintf(stderr, "%s:%d:%d: runtime error: %s\n",
            file.c_str(), e.loc.line, e.loc.column, e.what());
        return 1;
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "%s: runtime error: %s\n", file.c_str(), e.what());
        return 1;
    }
    return 0;
}