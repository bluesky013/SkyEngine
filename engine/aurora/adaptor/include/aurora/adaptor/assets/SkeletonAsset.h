//
// Aurora skeleton asset (bridge): wraps the animation module's SkeletonData.
// aurora/core does NOT define a Skeleton; this adaptor-level asset is the
// bridge that links `Animation` and turns a SkeletonData payload into a runtime
// Skeleton. Cooked from `.skeleton` sources (or emitted by the mesh cook when a
// source is skinned).
//

#pragma once

#include <animation/core/Skeleton.h>
#include <core/math/Transform.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    struct SkeletonAssetData : public SkeletonData {
        static constexpr uint32_t CURRENT_VERSION = 1;

        uint32_t version = CURRENT_VERSION;

        void Save(BinaryOutputArchive &ar) const
        {
            ar.SaveValue(version);
            ar.SaveValue(static_cast<uint32_t>(boneData.size()));
            for (const auto &bone : boneData) {
                ar.SaveValue(std::string(bone.name.GetStr()));
                ar.SaveValue(bone.parentIndex);
            }
            ar.SaveValue(static_cast<uint32_t>(refPos.size()));
            for (const auto &transform : refPos) {
                ar.SaveValue(reinterpret_cast<const char *>(&transform), sizeof(Transform));
            }
        }

        void Load(BinaryInputArchive &ar)
        {
            ar.LoadValue(version);
            if (version != CURRENT_VERSION) {
                clear();
                return;
            }
            uint32_t count = 0;
            ar.LoadValue(count);
            boneData.resize(count);
            for (auto &bone : boneData) {
                std::string name;
                ar.LoadValue(name);
                bone.name = Name(name.c_str());
                ar.LoadValue(bone.parentIndex);
            }
            ar.LoadValue(count);
            refPos.resize(count);
            for (auto &transform : refPos) {
                ar.LoadValue(reinterpret_cast<char *>(&transform), sizeof(Transform));
            }
        }

        void clear()
        {
            boneData.clear();
            refPos.clear();
        }
    };

} // namespace sky::aurora

// Specialization must precede any use of Asset<sky::Skeleton>.
namespace sky {

    template <>
    struct AssetTraits<sky::Skeleton> {
        using DataType                                   = sky::aurora::SkeletonAssetData;
        static constexpr std::string_view ASSET_TYPE     = "AuroraSkeleton";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky

namespace sky::aurora {

    // Build (or reuse) the runtime Skeleton for an asset.
    inline SkeletonPtr FetchOrCreateSkeletonByAsset(const std::shared_ptr<Asset<Skeleton>> &asset)
    {
        if (asset == nullptr) {
            return {};
        }
        return asset->GetOrCreateResource([](Asset<Skeleton> &value) { return Skeleton::BuildSkeleton(value.Data()); });
    }

} // namespace sky::aurora
