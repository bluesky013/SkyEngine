//
// Created by Zach Lee on 2022/8/25.
//

#pragma once

#include <render/resources/Mesh.h>
#include <framework/serialization/BasicSerialization.h>
#include <core/util/Uuid.h>
#include <core/math/Matrix.h>
#include <unordered_map>
#include <string>

namespace sky {

    struct PrefabNode {
        Matrix4 transform;
        uint32_t parentIndex = ~(0u);
        uint32_t meshIndex = ~(0u);

        template<class Archive>
        void serialize(Archive &ar)
        {
            ar(transform, parentIndex);
        }
    };

    struct PrefabData {
        std::vector<PrefabNode> nodes;
        std::unordered_map<Uuid, std::string> assetPathMap;

        template<class Archive>
        void serialize(Archive &ar)
        {
            ar(nodes, assetPathMap);
        }
    };

    class Prefab {
    public:
        Prefab() = default;
        ~Prefab() = default;
    };
    using PrefabPtr = std::shared_ptr<Prefab>;

    namespace impl {
        void LoadFromPath(const std::string& path, PrefabData& data);
        void SaveToPath(const std::string& path, const PrefabData& data);
        PrefabPtr CreateFromData(const PrefabData& data);
    }

    template <>
    struct AssetTraits <Prefab> {
        using DataType = PrefabData;

        static void LoadFromPath(const std::string& path, DataType& data)
        {
            impl::LoadFromPath(path, data);
        }

        static PrefabPtr CreateFromData(const DataType& data)
        {
            return impl::CreateFromData(data);
        }

        static void SaveToPath(const std::string& path, const DataType& data)
        {
            impl::SaveToPath(path, data);
        }
    };
}