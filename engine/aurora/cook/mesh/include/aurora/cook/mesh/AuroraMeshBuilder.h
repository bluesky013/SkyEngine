//
// Aurora mesh builder: AssetBuilder implementation cooking mesh sources
// (gltf/glb/fbx/obj) into AuroraMesh (+ AuroraSkin for skinned sources).
// Mirrors AuroraImageBuilder.
//

#pragma once

#include <aurora/cook/mesh/MeshBuildConfig.h>
#include <framework/asset/AssetBuilder.h>

#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    class AuroraMeshBuilder : public AssetBuilder {
    public:
        AuroraMeshBuilder()           = default;
        ~AuroraMeshBuilder() override = default;

        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &) const override;
        void LoadConfig(const FileSystemPtr &cfg) override;
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

    private:
        std::vector<std::string> extensions = {".gltf", ".glb", ".fbx", ".obj"};
        cook::MeshBuildPresets   presets;
    };

} // namespace sky::aurora
