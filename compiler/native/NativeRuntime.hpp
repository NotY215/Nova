#pragma once
// Native runtime shims — these are the C-linkage entry points that
// LLVM-generated code calls into.  They are registered with the JIT
// via DynamicLibrarySearchGenerator so the JIT-compiled module can
// resolve them against the running process.

#include <cstdint>

extern "C" {
    void vayu_print_int(long long v);
    void vayu_print_bool(bool v);
    void vayu_print_ln();   // newline only
}