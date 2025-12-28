//
// Created by blues on 2024/8/1.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/math/Matrix4.h>
#include <core/math/Transform.h>
#include <core/name/Name.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <animation/core/AnimationPose.h>

namespace sky {
    using BoneIndex = uint16_t;
    static const BoneIndex INVALID_BONE_ID = std::numeric_limits<BoneIndex>::max();

    struct BoneData {
        Name name;
        BoneIndex parentIndex = INVALID_BONE_ID;
    };

    struct SkeletonData {
        std::vector<BoneData> boneData;
        std::vector<Matrix4> inverseBindMatrix;
        std::vector<Transform> refPos;
    };

    class Skeleton;
    using SkeletonPtr = CounterPtr<Skeleton>;

    struct Bone {
        Name name;
        BoneIndex index = 0;
        BoneIndex parent = INVALID_BONE_ID;
        std::vector<BoneIndex> children;
    };

    class Skeleton : public RefObject {
    public:
        Skeleton() = default;
        ~Skeleton() override = default;

        static SkeletonPtr BuildSkeleton(const SkeletonData& data);

        FORCEINLINE const std::vector<Bone*> &GetRoots() const { return roots; }
        FORCEINLINE const Bone* GetBoneByIndex(BoneIndex index) const { return bones[index].get(); }
        FORCEINLINE const PoseSharedPtr &GetRefPos() const { return refPos; }
        const Bone* GetBoneByName(const Name& name) const;

    private:
        void UpdateBoneTree();

        std::vector<Bone*> roots;
        std::vector<std::unique_ptr<Bone>> bones;
        std::vector<Matrix4> inverseBindMatrix;
        std::unordered_map<Name, BoneIndex> nameToIndexMap;

        PoseSharedPtr refPos;
    };

} // namespace sky
