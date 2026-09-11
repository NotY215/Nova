#include "NativeRuntime.hpp"
#include <cstdio>

extern "C" {

    void vayu_print_int(long long v) {
        std::printf("%lld\n", v);
    }

    void vayu_print_bool(bool v) {
        std::printf("%s\n", v ? "true" : "false");
    }

    void vayu_print_ln() {
        std::printf("\n");
    }

} // extern "C"