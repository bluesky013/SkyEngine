// The adaptor owns the asset bridge; Skeleton remains renderer-independent.

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

        uint32_t        version = CURRENT_VERSION;
        AnimationStatus validation;
        bool            IsLoadValid() const
        {
            return static_cast<bool>(validation);
        }

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
            clear();
            ar.LoadValue(version);
            if (version != CURRENT_VERSION) {
                validation = {AnimationError::UnsupportedVersion, "unsupported skeleton product version; recook the source"};
                return;
            }
            uint32_t count = 0;
            ar.LoadValue(count);
            if (count == 0 || count > ANIM_MAX_BONE || !ar.IsValid()) {
                validation = {AnimationError::InvalidSkeleton, "skeleton requires 1..256 bones"};
                return;
            }
            boneData.resize(count);
            for (auto &bone : boneData) {
                std::string name;
                uint32_t    length = 0;
                ar.LoadValue(length);
                if (length == 0 || length > 4096 || !ar.IsValid()) {
                    validation = {AnimationError::InvalidSkeleton, "invalid skeleton bone name length"};
                    return;
                }
                name.resize(length);
                ar.LoadValue(name.data(), length);
                if (name.find('\0') != std::string::npos) {
                    validation = {AnimationError::InvalidSkeleton, "bone name contains a null byte"};
                    return;
                }
                bone.name = Name(name.c_str());
                ar.LoadValue(bone.parentIndex);
            }
            ar.LoadValue(count);
            if (count != boneData.size() || !ar.IsValid()) {
                validation = {AnimationError::InvalidSkeleton, "skeleton reference transform count mismatch"};
                return;
            }
            refPos.resize(count);
            for (auto &transform : refPos) {
                ar.LoadValue(reinterpret_cast<char *>(&transform), sizeof(Transform));
            }
            validation = ar.IsValid() ? Skeleton::Validate(*this)
                                      : AnimationStatus{AnimationError::InvalidSkeleton, "truncated skeleton reference transforms"};
        }

        void clear()
        {
            boneData.clear();
            refPos.clear();
            validation = {};
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
    inline SkeletonPtr FetchOrCreateSkeletonByAsset(const std::shared_ptr<Asset<Skeleton>> &asset, AnimationStatus *status = nullptr)
    {
        if (asset == nullptr) {
            if (status != nullptr) {
                *status = {AnimationError::InvalidAsset, "skeleton asset is missing"};
            }
            return {};
        }
        const auto validation = asset->Data().version != SkeletonAssetData::CURRENT_VERSION
                                    ? AnimationStatus{AnimationError::UnsupportedVersion, "unsupported skeleton product version; recook the source"}
                                    : asset->Data().validation;
        if (status != nullptr) {
            *status = validation;
        }
        if (!validation) {
            return {};
        }
        return asset->GetOrCreateResource([status](Asset<Skeleton> &value) { return Skeleton::BuildSkeleton(value.Data(), status); });
    }

} // namespace sky::aurora
