//
// Created by blues on 2024/8/1.
//

#include <animation/core/Skeleton.h>
#include <bit>
#include <cmath>
#include <string_view>
#include <unordered_set>

namespace sky {

    void Skeleton::UpdateBoneTree()
    {
        for (auto &bone : bones) {
            if (bone->parent == INVALID_BONE_ID) {
                roots.emplace_back(bone.get());
            } else {
                bones[bone->parent]->children.emplace_back(bone->index);
            }
        }
    }

    AnimationStatus Skeleton::Validate(const SkeletonData &data)
    {
        const auto count = data.boneData.size();
        if (count == 0 || count > ANIM_MAX_BONE || data.refPos.size() != count) {
            return {AnimationError::InvalidSkeleton, "skeleton requires 1..256 bones and one reference transform per bone"};
        }
        std::unordered_set<Name> names;
        for (size_t i = 0; i < count; ++i) {
            const auto &bone = data.boneData[i];
            if (std::string_view(bone.name.GetStr()).empty() || !names.emplace(bone.name).second) {
                return {AnimationError::InvalidSkeleton, "bone names or canonical paths must be nonempty and unique"};
            }
            if (bone.parentIndex != INVALID_BONE_ID && bone.parentIndex >= count) {
                return {AnimationError::InvalidSkeleton, "bone parent index is outside the skeleton"};
            }
            const auto &transform = data.refPos[i];
            for (size_t axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(transform.translation[axis]) || !std::isfinite(transform.scale[axis]) || transform.scale[axis] == 0.f) {
                    return {AnimationError::InvalidSkeleton, "reference TRS must be finite and invertible"};
                }
            }
            double rotationLength = 0.0;
            for (size_t axis = 0; axis < 4; ++axis) {
                const float value = VectorTraits<Quaternion>::Visit(transform.rotation, axis);
                if (!std::isfinite(value)) {
                    return {AnimationError::InvalidSkeleton, "reference rotation must be finite"};
                }
                rotationLength += static_cast<double>(value) * value;
            }
            if (std::abs(rotationLength - 1.0) > 1e-4) {
                return {AnimationError::InvalidSkeleton, "reference rotation must be a unit quaternion"};
            }
        }
        for (size_t i = 0; i < count; ++i) {
            BoneIndex current = static_cast<BoneIndex>(i);
            size_t    visited = 0;
            while (current != INVALID_BONE_ID) {
                if (++visited > count) {
                    return {AnimationError::InvalidSkeleton, "skeleton parent cycle"};
                }
                current = data.boneData[current].parentIndex;
            }
        }
        return {};
    }

    SkeletonPtr Skeleton::BuildSkeleton(const SkeletonData &data, AnimationStatus *status)
    {
        const auto validation = Validate(data);
        if (status != nullptr) {
            *status = validation;
        }
        if (!validation) {
            return {};
        }
        SkeletonPtr skeleton  = new Skeleton();
        uint64_t    signature = 14695981039346656037ULL;
        const auto  hashByte  = [&signature](uint8_t value) { signature = (signature ^ value) * 1099511628211ULL; };
        const auto  hashWord  = [&hashByte](uint32_t value) {
            for (uint32_t shift = 0; shift < 32; shift += 8) {
                hashByte(static_cast<uint8_t>(value >> shift));
            }
        };
        const auto hashFloat = [&hashWord](float value) { hashWord(std::bit_cast<uint32_t>(value == 0.f ? 0.f : value)); };

        skeleton->bones.resize(data.boneData.size());
        for (size_t i = 0; i < data.boneData.size(); ++i) {
            const auto index = static_cast<BoneIndex>(i);

            skeleton->bones[i]         = std::make_unique<Bone>();
            skeleton->bones[i]->index  = index;
            skeleton->bones[i]->name   = data.boneData[i].name;
            skeleton->bones[i]->parent = data.boneData[i].parentIndex;

            skeleton->nameToIndexMap.emplace(data.boneData[i].name, index);
            const std::string_view name(data.boneData[i].name.GetStr());
            hashWord(static_cast<uint32_t>(name.size()));
            for (const char value : name) {
                hashByte(static_cast<uint8_t>(value));
            }
            hashWord(data.boneData[i].parentIndex);
            const auto &transform = data.refPos[i];
            for (size_t axis = 0; axis < 3; ++axis) {
                hashFloat(transform.translation[axis]);
                hashFloat(transform.scale[axis]);
            }
            for (size_t axis = 0; axis < 4; ++axis) {
                hashFloat(VectorTraits<Quaternion>::Visit(transform.rotation, axis));
            }
        }
        skeleton->layoutSignature = signature;
        skeleton->UpdateBoneTree();

        skeleton->refPos             = std::make_shared<AnimPose>();
        skeleton->refPos->transforms = data.refPos;
        skeleton->refPos->skeleton   = skeleton.Get();
        return skeleton;
    }

    const Bone *Skeleton::GetBoneByName(const Name &name) const
    {
        auto iter = nameToIndexMap.find(name);
        return iter != nameToIndexMap.end() ? GetBoneByIndex(iter->second) : nullptr;
    }

} // namespace sky
