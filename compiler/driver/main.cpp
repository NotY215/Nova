#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "ast/Ast.hpp"
#include "sema/TypeChecker.hpp"
#include "interp/Interpreter.hpp"
#include "compile/Compiler.hpp"
#include "vm/VM.hpp"
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
    std::stringstream ss; ss << in.rdbuf();
    return ss.str();
}

static void usage() {
    std::fprintf(stderr,
        "usage: vayuc <file.vayu> [--run | --check | --dump-tokens | --dump-ast | --dump-bytecode]\n"
        "       --run            type-check then execute (default)\n"
        "       --check          type-check only\n"
        "       --dump-tokens    print lexer output\n"
        "       --dump-ast       print parsed AST\n"
        "       --dump-bytecode  compile to bytecode and print disassembly\n"
        "       --vm             run on the bytecode VM instead of the tree-walker\n"
        "       --no-check       skip the type checker\n");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc < 2) { usage(); return 1; }

    std::string file = argv[1];
    enum class Mode { Run, Check, DumpTokens, DumpAst, DumpBytecode } mode = Mode::Run;
    bool skipCheck = false;
    bool useVM = false;

    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--run"))           mode = Mode::Run;
        else if (!std::strcmp(argv[i], "--check"))         mode = Mode::Check;
        else if (!std::strcmp(argv[i], "--dump-tokens"))   mode = Mode::DumpTokens;
        else if (!std::strcmp(argv[i], "--dump-ast"))      mode = Mode::DumpAst;
        else if (!std::strcmp(argv[i], "--dump-bytecode")) mode = Mode::DumpBytecode;
        else if (!std::strcmp(argv[i], "--no-check"))      skipCheck = true;
        else if (!std::strcmp(argv[i], "--vm"))            useVM = true;
        else { std::fprintf(stderr, "vayuc: unknown flag '%s'\n", argv[i]); return 1; }
    }

    std::string src = readFile(file);
    if (src.empty()) {
        std::fprintf(stderr, "vayuc: cannot read '%s'\n", file.c_str());
        return 1;
    }

    vayu::Lexer lexer(std::move(src));
    auto tokens = lexer.tokenize();

    if (mode == Mode::DumpTokens) {
        for (const auto& t : tokens)
            std::printf("%3d:%-3d  %-14s  %s\n",
                t.location.line, t.location.column,
                vayu::tokenTypeName(t.type), t.lexeme.c_str());
        return 0;
    }

    vayu::Block program;
    try {
        vayu::Parser parser(std::move(tokens));
        program = parser.parseProgram();
    }
    catch (const vayu::ParseError& e) {
        std::fprintf(stderr, "%s:%d:%d: parse error: %s\n",
            file.c_str(), e.loc.line, e.loc.column, e.what());
        return 1;
    }

    if (mode == Mode::DumpAst) { vayu::printProgram(program); return 0; }

    if (!skipCheck && mode != Mode::DumpBytecode) {
        try {
            vayu::TypeChecker checker;
            checker.check(program);
        }
        catch (const vayu::TypeError& e) {
            std::fprintf(stderr, "%s:%d:%d: type error: %s\n",
                file.c_str(), e.loc.line, e.loc.column, e.what());
            return 1;
        }
    }

    if (mode == Mode::Check) {
        std::printf("OK: %s type-checks successfully.\n", file.c_str());
        return 0;
    }

    // ---- Bytecode dump mode ----
    if (mode == Mode::DumpBytecode) {
        auto chunk = std::make_shared<vayu::Chunk>();
        try {
            vayu::Compiler c;
            c.compile(program, *chunk);
        }
        catch (const vayu::CompileError& e) {
            std::fprintf(stderr, "%s:%d:%d: compile error: %s\n",
                file.c_str(), e.loc.line, e.loc.column, e.what());
            return 1;
        }
        vayu::disassemble(*chunk, file.c_str());
        return 0;
    }

    // Source directory for module resolution.
    std::string srcDir;
    {
        auto slash = file.find_last_of("/\\");
        if (slash != std::string::npos) srcDir = file.substr(0, slash + 1);
    }

    // ---- Run on bytecode VM ----
    if (useVM) {
        auto chunk = std::make_shared<vayu::Chunk>();
        try {
            vayu::Compiler c;
            c.compile(program, *chunk);
        }
        catch (const vayu::CompileError& e) {
            std::fprintf(stderr, "%s:%d:%d: VM compile error: %s\n",
                file.c_str(), e.loc.line, e.loc.column, e.what());
            return 1;
        }
        try {
            vayu::Interpreter interp;
            interp.setSourceDir(srcDir);
            interp.registerDeclarations(program);   // <-- NEW: register classes/structs
            vayu::VM vm(interp.globals());
            vm.run(chunk);
        }
        catch (const vayu::VMRuntimeError& e) {
            std::fprintf(stderr, "%s:%d: VM runtime error: %s\n",
                file.c_str(), e.line, e.what());
            return 1;
        }
        catch (const std::exception& e) {
            std::fprintf(stderr, "%s: VM error: %s\n", file.c_str(), e.what());
            return 1;
        }
        return 0;
    }

    // ---- Run on tree-walker (default) ----
    try {
        vayu::Interpreter interp;
        interp.setSourceDir(srcDir);
        interp.run(program);
    }
    catch (const vayu::VayuException& e) {
        std::fprintf(stderr, "%s:%d:%d: uncaught %s: %s\n",
            file.c_str(), e.loc.line, e.loc.column,
            vayu::Interpreter::exceptionTypeName(e.value).c_str(),
            vayu::Interpreter::exceptionMessage(e.value).c_str());
        return 1;
    }
    catch (const vayu::RuntimeError& e) {
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