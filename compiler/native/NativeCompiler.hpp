#pragma once
#include "ast/Ast.hpp"
#include <string>

namespace vayu {

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
        std::string qbeTarget_;   // e.g. "amd64_win" or "amd64_sysv"

        std::string buildQBE(const Block& program);
        bool        writeRuntimeC(const std::string& path) const;
    };

} // namespace vayu