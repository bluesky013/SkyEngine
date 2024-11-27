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

    static const uint32_t INVALID_BONE_ID = ~(0U);

    struct BoneData {
        Name name;
        uint32_t parentIndex = INVALID_BONE_ID;
    };

    struct Bone {
        uint32_t index = 0;
        Bone* parent = nullptr;
        std::vector<Bone*> children;
    };

    struct SkeletonData {
        std::vector<BoneData> boneData;
        std::vector<Matrix4> inverseBindMatrix;
        std::vector<Transform> refPos;
        std::unordered_map<Name, uint32_t> nameToIndexMap;
    };

    class Skeleton : public RefObject {
    public:
        Skeleton() = default;
        ~Skeleton() override = default;

    private:
        PosePtr refPose;

        Bone root = {};
        std::vector<Bone> bones;

        std::vector<BoneData> boneData;
        std::vector<Matrix4> inverseBindMatrix;
        std::unordered_map<Name, uint32_t> nameToIndexMap;
    };
    using SkeletonPtr = CounterPtr<Skeleton>;

} // namespace sky