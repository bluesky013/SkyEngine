//
// Created by blues on 2024/8/1.
//

#pragma once

#include <animation/core/AnimationPose.h>
#include <animation/core/AnimationStatus.h>
#include <core/math/Transform.h>
#include <core/name/Name.h>
#include <core/template/ReferenceObject.h>
#include <memory>
#include <unordered_map>
#include <vector>

namespace sky {
    using BoneIndex                        = uint16_t;
    static const BoneIndex INVALID_BONE_ID = std::numeric_limits<BoneIndex>::max();

    struct BoneData {
        Name      name;
        BoneIndex parentIndex = INVALID_BONE_ID;
    };

    struct SkeletonData {
        std::vector<BoneData>  boneData;
        std::vector<Transform> refPos;
    };

    class Skeleton;
    using SkeletonPtr = CounterPtr<Skeleton>;

    struct Bone {
        Name                   name;
        BoneIndex              index  = 0;
        BoneIndex              parent = INVALID_BONE_ID;
        std::vector<BoneIndex> children;
    };

    class Skeleton : public RefObject {
    public:
        Skeleton()           = default;
        ~Skeleton() override = default;

        static AnimationStatus Validate(const SkeletonData &data);
        static SkeletonPtr     BuildSkeleton(const SkeletonData &data, AnimationStatus *status = nullptr);

        FORCEINLINE const std::vector<Bone *> &GetRoots() const
        {
            return roots;
        }
        FORCEINLINE const Bone *GetBoneByIndex(BoneIndex index) const
        {
            return bones[index].get();
        }
        FORCEINLINE const PoseSharedPtr &GetRefPos() const
        {
            return refPos;
        }
        const Bone *GetBoneByName(const Name &name) const;
        size_t      GetNumBones() const
        {
            return bones.size();
        }
        uint64_t GetLayoutSignature() const
        {
            return layoutSignature;
        }

    private:
        void UpdateBoneTree();

        std::vector<Bone *>                 roots;
        std::vector<std::unique_ptr<Bone>>  bones;
        std::unordered_map<Name, BoneIndex> nameToIndexMap;

        PoseSharedPtr refPos;
        uint64_t      layoutSignature = 0;
    };

} // namespace sky
