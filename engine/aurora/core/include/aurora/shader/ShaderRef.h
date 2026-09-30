//
// ShaderRef: shader identity by source-relative path (not a Uuid / asset).
// Used by material / pass to reference a shader; the resolver turns it into a
// concrete RHI Shader via the source/cache pipeline.
//

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace sky::aurora {

    struct ShaderRef {
        std::string relativePath;
        std::string entry;
    };

    // Reserved key for the (future) technique/PSO cache: a shader + variant.
    // The renderer's TechniqueCache (aurora-renderer / aurora-material-pso) will
    // extend this with vertex layout + attachment formats.
    struct TechniqueKey {
        ShaderRef shader;
        uint64_t  variantHash = 0;
        uint32_t  target      = 0;
    };

    // Normalize a shader relative path for use as a cache key: forward slashes,
    // collapsed separators, no leading "./". Case is preserved.
    std::string NormalizeShaderPath(std::string_view path);

} // namespace sky::aurora
