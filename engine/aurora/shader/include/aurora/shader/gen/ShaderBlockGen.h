//
// ShaderBlockGen: RgBlockDesc -> HLSL header text.
// Pure library function shared by runtime and the offline shader cache builder
// (virtual include via ShaderFileSystem; nothing written to disk).
//

#pragma once

#include <aurora/shader/RgBlockDesc.h>

#include <string>

namespace sky::aurora {

    class ShaderBlockGen {
    public:
        // generate HLSL declaration text for the block
        static std::string GenerateHlsl(const RgBlockDesc &desc);

        // content hash of the generated header; the offline shader cache key
        // MUST include this so header changes invalidate the cache
        static uint32_t ContentHash(const RgBlockDesc &desc);
    };

} // namespace sky::aurora
