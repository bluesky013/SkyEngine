//
// Shared CPU-side push constant staging for the Metal graphics/compute encoders.
//

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace sky::aurora {

    // slang lowers push constants to a plain constant buffer, and MTL set*Bytes
    // has no base offset, so a partial-range push cannot be expressed directly.
    // Writes accumulate into a CPU block (grown to at least the shader's declared
    // block size) and the whole block is re-uploaded on every call; blocks are
    // tiny. Returns the block to upload and reports its length via `outLength`.
    inline const void *AccumulatePushConstant(std::vector<uint8_t> &block, uint32_t blockSize, uint32_t offset,
                                              uint32_t size, const void *data, uint32_t &outLength)
    {
        const uint32_t required = blockSize > (offset + size) ? blockSize : (offset + size);
        if (block.size() < required) {
            block.resize(required, 0);
        }
        std::memcpy(block.data() + offset, data, size);
        outLength = static_cast<uint32_t>(block.size());
        return block.data();
    }

} // namespace sky::aurora
