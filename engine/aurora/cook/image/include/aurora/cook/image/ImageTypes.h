//
// RHI-free cook image enums shared by the build config and the pixel pipeline. Kept separate from
// ImageProcess.h (which pulls in aurora/rhi) so ImageBuildConfig stays independent of the RHI.
//

#pragma once

#include <cstdint>

namespace sky::aurora::cook {

    enum class PixelType : uint32_t {
        U8,
        HALF,
        Float,
    };

    enum class MipGenType : uint32_t { Box, Kaiser, Lanczos3 };

    enum class Quality : uint32_t { ULTRA_FAST, VERY_FAST, FAST, BASIC, SLOW };

} // namespace sky::aurora::cook
