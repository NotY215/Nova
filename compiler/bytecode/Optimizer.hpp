#pragma once
#include "bytecode/Chunk.hpp"

namespace vayu {

    struct OptStats {
        int constantsFolded = 0;
        int notNotCollapsed = 0;
        int peepholesApplied = 0;
    };

    void optimizeChunk(Chunk& chunk, OptStats& stats);
    void optimizeChunk(Chunk& chunk);

} // namespace vayu