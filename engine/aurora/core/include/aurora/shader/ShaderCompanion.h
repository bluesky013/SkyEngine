//
// ShaderCompanion: a shader's variant schema authored next to its source as a
// companion `.slang.json`. The resolver reads it (source wins over the offline
// index) and folds it into the source hash.
//

#pragma once

#include <aurora/shader/ShaderVariant.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    class ShaderFileSystem;

    struct ShaderCompanion {
        ShaderVariantSchema           schema;
        std::vector<VertexVariantDef> vertexDefs;
        std::vector<std::string>      entryPoints; // declared entry points
        std::vector<std::string>      depends;     // extra source deps
        uint64_t                      fingerprint = 0;
    };

    // Parse a companion JSON document into schema + vertex defs.
    bool ParseShaderCompanion(std::string_view json, ShaderCompanion &out, std::string *error = nullptr);

    // Resolve + parse the companion next to `relativePath` (i.e.
    // `relativePath + ".json"`) via `fs`. Returns false when absent or invalid.
    bool LoadShaderCompanion(ShaderFileSystem &fs, const std::string &relativePath, ShaderCompanion &out, std::string *error = nullptr);

} // namespace sky::aurora
