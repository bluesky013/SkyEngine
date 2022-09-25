//
// Created by Zach Lee on 2022/8/25.
//

#pragma once

#include <core/math/Matrix4.h>
#include <core/util/Uuid.h>
#include <framework/serialization/BasicSerialization.h>
#include <render/resources/Mesh.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace sky {

    class RenderScene;

    struct PrefabAssetNode {
        Matrix4  transform;
        uint32_t meshIndex = ~(0u);
        uint32_t parentIndex = ~(0u);
        std::vector<uint32_t> children;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(transform, meshIndex, parentIndex, children);
        }
    };

    struct PrefabData {
        std::vector<PrefabAssetNode> nodes;
        std::vector<Uuid> buffers;
        std::vector<Uuid> meshes;
        std::vector<Uuid> images;
        std::vector<Uuid> materials;
        std::unordered_map<Uuid, std::string> assetPathMap;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(nodes, buffers, meshes, images, materials, assetPathMap);
        }
    };

    // temp prefab only used for render.
    class Prefab {
    public:
        Prefab()  = default;
        ~Prefab() = default;

        static std::shared_ptr<Prefab> CreateFromData(const PrefabData &data);

        void LoadToScene(RenderScene &scene);

    private:
        std::vector<PrefabAssetNode> nodes;
        std::vector<BufferAssetPtr> buffers;
        std::vector<MeshAssetPtr> meshes;
        std::vector<ImageAssetPtr> images;
        std::vector<MaterialAssetPtr> materials;
    };
    using PrefabPtr = std::shared_ptr<Prefab>;

    template <>
    struct AssetTraits<Prefab> {
        using DataType                                = PrefabData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("B3F0BC22-EAF6-47BA-9A8F-3D5F11D06777");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;

        static PrefabPtr CreateFromData(const DataType &data)
        {
            return Prefab::CreateFromData(data);
        }
    };
} // namespace sky