#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "ast/Ast.hpp"
#include "sema/TypeChecker.hpp"
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
        "usage: novac <file.nova> [--run | --check | --dump-tokens | --dump-ast | --no-check]\n"
        "       --run         type-check then execute (default)\n"
        "       --check       type-check only, do not execute\n"
        "       --no-check    run without static type checking\n"
        "       --dump-tokens print lexer output\n"
        "       --dump-ast    print parsed AST\n");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc < 2) { usage(); return 1; }

    std::string file = argv[1];
    enum class Mode { Run, Check, DumpTokens, DumpAst } mode = Mode::Run;
    bool skipCheck = false;

    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--run"))         mode = Mode::Run;
        else if (!std::strcmp(argv[i], "--check"))       mode = Mode::Check;
        else if (!std::strcmp(argv[i], "--dump-tokens")) mode = Mode::DumpTokens;
        else if (!std::strcmp(argv[i], "--dump-ast"))    mode = Mode::DumpAst;
        else if (!std::strcmp(argv[i], "--no-check"))    skipCheck = true;
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
        std::fprintf(stderr, "%s:%d:%d: parse error: %s\n",
            file.c_str(), e.loc.line, e.loc.column, e.what());
        return 1;
    }

    if (mode == Mode::DumpAst) {
        nova::printProgram(program);
        return 0;
    }

    // Type-check (unless skipped)
    if (!skipCheck) {
        try {
            nova::TypeChecker checker;
            checker.check(program);
        }
        catch (const nova::TypeError& e) {
            std::fprintf(stderr, "%s:%d:%d: type error: %s\n",
                file.c_str(), e.loc.line, e.loc.column, e.what());
            return 1;
        }
    }

    if (mode == Mode::Check) {
        std::printf("OK: %s type-checks successfully.\n", file.c_str());
        return 0;
    }

    // Execute
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