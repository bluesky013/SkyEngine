//
// GlobalVariantLayout: the global pipeline variant layout, shared across all
// shaders. Mirrors GlobalRenderResources for the variant space: the pipeline
// module loads it from the pipeline variant .slang (data-driven) and validates
// that the keys fit within the reserved bit region.
//

#pragma once

#include <aurora/shader/ShaderVariant.h>

#include <cstdint>
#include <string>

namespace sky::aurora {

    class ShaderFileSystem;

    class GlobalVariantLayout {
    public:
        // reserved pipeline bits (budget), read from `@reserved` at load time
        uint16_t reservedBits = 0;

        // `@source pipeline` keys (relative bit offsets), loaded from data
        ShaderVariantSchema schema;

        // validate: pipeline key bits must fit within `reservedBits`
        bool Init(std::string *error = nullptr);

        // load + parse + validate from a pipeline variant .slang via a
        // ShaderFileSystem (same resolution as shader loading)
        bool Load(ShaderFileSystem &fs, const std::string &path, std::string *error = nullptr);

        const ShaderVariantSchema::Entry *Find(Name key) const;

        // dump the pipeline bits of a composed key
        std::string ToString(const ShaderVariantKey &key) const;

        // dump pipeline bits + per-shader bits of a composed key
        std::string ToString(const ShaderVariantKey &key,
                             const ShaderVariantSchema &perShaderSchema) const;
    };

} // namespace sky::aurora
