//
// CreateSkinFromMeshAsset: runtime-only build of the mesh-side aurora::Skin from
// the skin section absorbed into the mesh product bulk (see MeshAsset.h).
//

#include <aurora/adaptor/assets/MeshAsset.h>

#include <core/logger/Logger.h>

#include <cstring>
#include <vector>

static const char *TAG = "AuroraMeshAsset";

namespace sky::aurora {

    CounterPtr<Skin> CreateSkinFromMeshAsset(const Asset<Mesh> &asset)
    {
        const MeshAssetData &data = asset.Data();
        if (!data.HasSkin()) {
            return {};
        }

        const uint64_t expected =
            static_cast<uint64_t>(data.skinBoneCount) * sizeof(Matrix4) + static_cast<uint64_t>(data.skinMappingCount) * sizeof(uint32_t);
        if (data.skinSize != expected) {
            LOG_E(TAG, "skin section size mismatch (size=%llu expected=%llu)", static_cast<unsigned long long>(data.skinSize),
                  static_cast<unsigned long long>(expected));
            return {};
        }

        const auto bytes = asset.ReadBulk(data.skinOffset, data.skinSize);
        if (bytes.size() != data.skinSize) {
            LOG_E(TAG, "skin section out of range (offset=%llu size=%llu)", static_cast<unsigned long long>(data.skinOffset),
                  static_cast<unsigned long long>(data.skinSize));
            return {};
        }

        auto                 skin = CounterPtr<Skin>(new Skin());
        std::vector<Matrix4> matrices(data.skinBoneCount);
        if (!matrices.empty()) {
            std::memcpy(matrices.data(), bytes.data(), matrices.size() * sizeof(Matrix4));
        }
        skin->SetInverseBindMatrices(std::move(matrices));

        if (data.skinMappingCount != 0) {
            std::vector<uint32_t> mapping(data.skinMappingCount);
            std::memcpy(mapping.data(), bytes.data() + static_cast<size_t>(data.skinBoneCount) * sizeof(Matrix4), mapping.size() * sizeof(uint32_t));
            skin->SetBoneMapping(std::move(mapping));
        }
        return skin;
    }

} // namespace sky::aurora
