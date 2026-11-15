//
// Mesh cook build config + bundle presets (mirrors ImageBuildConfig.h).
//

#pragma once

#include <core/util/Uuid.h>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora::cook {

    struct MeshCookStatus;

    struct MeshBuildConfig {
        Uuid defaultMaterial;
        bool tangents = true; // keep tangent attribute when the source has one
        bool optimize = true; // meshopt vertex cache / overdraw / vertex fetch
        bool skinning = true; // emit independent Skin and Skeleton products for skinned sources
    };

    MeshCookStatus ResolveMeshMaterials(const MeshBuildConfig &config, uint32_t slotCount, std::span<const Uuid> supplied, std::vector<Uuid> &out);

    struct MeshBuildPresets {
        std::string                            defaultBundle = "common";
        std::map<std::string, MeshBuildConfig> bundles{{"common", {}}};
        std::string                            loadError;

        MeshCookStatus LoadJson(std::string_view text);

        // resolves the config for a requested bundle; unknown keys fall back to
        // defaultBundle. Returns nullptr only when no bundles exist at all.
        const MeshBuildConfig *Resolve(const std::string &requested, std::string &resolvedKey) const;
    };

} // namespace sky::aurora::cook
