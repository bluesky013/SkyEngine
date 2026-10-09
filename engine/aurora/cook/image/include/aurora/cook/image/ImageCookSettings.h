//
// Reflected, editor-facing view of the per-bundle image cook settings.
// Member names match the sparse override keys persisted in the source manifest
// (`cook.settings[target]`): encode / srgb / quality / block / maxSize / generateMip.
//

#pragma once

#include <aurora/cook/image/ImageBuildConfig.h>

#include <cstdint>

namespace sky::aurora::cook {

    struct ImageCookSettings {
        ImageEncode encode      = ImageEncode::NONE;
        bool        srgb        = true;
        Quality     quality     = Quality::FAST;
        uint32_t    block       = 4; // ASTC block size (4 / 8)
        uint32_t    maxSize     = 0; // 0 == unlimited
        bool        generateMip = true;
    };

    // Registers the enums and struct with the process-wide SerializationContext (idempotent), so the
    // editor can bind an instance to the generic reflected form.
    void RegisterImageCookSettings();

} // namespace sky::aurora::cook
