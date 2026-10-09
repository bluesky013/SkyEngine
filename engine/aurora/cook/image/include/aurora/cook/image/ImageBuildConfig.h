//
// Image cook configuration: bundle -> encode settings, loaded from
// configs/image_build_presets.json. Drives the per-product-bundle output
// format (tex_pc -> BC7, tex_mobile -> ASTC, ...).
//

#pragma once

#include <aurora/cook/image/ImageTypes.h>
#include <framework/serialization/JsonArchive.h>

#include <cstdint>
#include <map>
#include <string>

namespace sky::aurora::cook {

    enum class ImageEncode : uint32_t {
        NONE,
        BC7,
        ASTC,
    };

    struct ImageBuildConfig {
        ImageEncode encode      = ImageEncode::NONE;
        bool        srgb        = true;
        Quality     quality     = Quality::FAST;
        uint32_t    astcBlock   = 4; // 4 or 8
        uint32_t    maxSize     = 0; // 0 == unlimited
        bool        generateMip = true;

        bool IsCompressed() const
        {
            return encode != ImageEncode::NONE;
        }

        // Overlay a sparse per-asset override (keys: encode/srgb/quality/block/maxSize/generateMip).
        // Unknown keys and unparsable values are ignored, keeping the preset value.
        void ApplyOverride(const std::map<std::string, std::string> &override);
    };

    struct ImageBuildPresets {
        std::string                             defaultBundle;
        std::map<std::string, ImageBuildConfig> bundles;

        void LoadJson(JsonInputArchive &json);

        // Unknown requested bundle falls back to defaultBundle; nullptr when no
        // config is available at all.
        const ImageBuildConfig *Resolve(const std::string &requested, std::string &resolvedKey) const;
    };

} // namespace sky::aurora::cook
