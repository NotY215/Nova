#pragma once
#include "ast/Ast.hpp"
#include <string>

namespace vayu {

    /// Compiles a Vayu AST to QBE IL, invokes `qbe` + `gcc`, and runs the
    /// resulting executable.
    class NativeCompiler {
    public:
        NativeCompiler();

        int  compileAndRun(const Block& program);
        void dumpIR(const Block& program);

        const std::string& lastError() const { return lastError_; }

        void setQbePath(const std::string& p) { qbePath_ = p; }
        void setCcPath(const std::string& p) { ccPath_ = p; }
        void setQbeTarget(const std::string& t) { qbeTarget_ = t; }

    private:
        std::string lastError_;
        std::string qbePath_;
        std::string ccPath_;
        std::string qbeTarget_;

        std::string buildQBE(const Block& program);
        bool        writeRuntimeC(const std::string& path) const;
    };

} // namespace vayu