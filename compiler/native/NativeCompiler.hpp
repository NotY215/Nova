#pragma once
#include "ast/Ast.hpp"
#include <memory>
#include <string>

namespace vayu {

    /// Compiles Vayu AST directly to LLVM IR and JIT-executes it.
    /// Frontend code never includes any LLVM headers — everything is
    /// hidden behind the pimpl.
    class NativeCompiler {
    public:
        NativeCompiler();
        ~NativeCompiler();

        /// Compile + JIT + run.  Returns 0 on success, non-zero on error.
        /// Prints diagnostics to stderr.
        int compileAndRun(const Block& program);

        /// Compile to LLVM IR and print the textual module to stdout.
        void dumpIR(const Block& program);

        const std::string& lastError() const { return lastError_; }

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
        std::string           lastError_;
    };

} // namespace vayu