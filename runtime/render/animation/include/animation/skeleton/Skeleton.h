//
// Created by blues on 2024/8/1.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/math/Matrix4.h>
#include <core/math/Transform.h>
#include <core/name/Name.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <animation/skeleton/Pose.h>

namespace sky {
    using BoneIndex = uint16_t;
    static const BoneIndex INVALID_BONE_ID = std::numeric_limits<BoneIndex>::max();

    struct BoneData {
        Name name;
        uint32_t parentIndex = INVALID_BONE_ID;
    };

    struct SkeletonData {
        std::vector<BoneData> boneData;
        std::vector<Matrix4> inverseBindMatrix;
        std::vector<Transform> refPos;
    };

    class Skeleton;

    struct Bone {
        BoneIndex index = 0;
        BoneIndex parent = INVALID_BONE_ID;
        std::vector<BoneIndex> children;
        Skeleton* skeleton = nullptr;
    };

    class Skeleton;
    using SkeletonPtr = CounterPtr<Skeleton>;

    class Skeleton : public RefObject {
    public:
        Skeleton() = default;
        ~Skeleton() override = default;

        static SkeletonPtr BuildSkeleton(const SkeletonData& data);

    private:
        void UpdateBoneTree();

        std::vector<Bone*> roots;
        std::vector<std::unique_ptr<Bone>> bones;
        std::unordered_map<Name, BoneIndex> nameToIndexMap;
    };

} // namespace sky
