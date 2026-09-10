#include "lexer/Lexer.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: novac <file.nova> [--dump-tokens]\n";
        return 1;
    }

    std::string src = readFile(argv[1]);
    if (src.empty()) {
        std::cerr << "novac: cannot read '" << argv[1] << "'\n";
        return 1;
    }

    nova::Lexer lexer(std::move(src));
    auto tokens = lexer.tokenize();

    for (const auto& t : tokens) {
        std::printf("%3d:%-3d  %-14s  %s\n",
            t.location.line, t.location.column,
            nova::tokenTypeName(t.type),
            t.lexeme.c_str());
    }
    return 0;
}