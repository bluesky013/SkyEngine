//
// Mesh cook build config + bundle presets (mirrors ImageBuildConfig.h).
//

#pragma once

#include <map>
#include <string>

namespace sky {
    class JsonInputArchive;
}

namespace sky::aurora::cook {

    struct MeshBuildConfig {
        bool tangents  = true;  // keep tangent attribute when the source has one
        bool optimize  = true;  // meshopt vertex cache / overdraw / vertex fetch
        bool meshlets  = false; // build meshlet payload (meshopt)
        bool skinning  = true;  // emit the AuroraSkin asset for skinned sources
    };

    struct MeshBuildPresets {
        std::string                        defaultBundle;
        std::map<std::string, MeshBuildConfig> bundles;

        void LoadJson(JsonInputArchive &json);

        // resolves the config for a requested bundle; unknown keys fall back to
        // defaultBundle. Returns nullptr only when no bundles exist at all.
        const MeshBuildConfig *Resolve(const std::string &requested, std::string &resolvedKey) const;
    };

} // namespace sky::aurora::cook
