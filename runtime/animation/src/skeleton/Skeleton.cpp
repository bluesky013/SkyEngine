//
// Created by blues on 2024/8/1.
//

#include <animation/skeleton/Skeleton.h>
#include <core/platform/Platform.h>

namespace sky {

    void Skeleton::UpdateBoneTree()
    {
        for (auto &bone : bones) {
            if (bone->parent == INVALID_BONE_ID) {
                roots.emplace_back(bone.get());
            } else {
                SKY_ASSERT(bone->parent < bones.size());
                bones[bone->parent]->children.emplace_back(bone->index);
            }
        }
    }

    SkeletonPtr Skeleton::BuildSkeleton(const SkeletonData& data)
    {
        auto* skeleton = new Skeleton();

        skeleton->inverseBindMatrix = data.inverseBindMatrix;
        skeleton->bones.resize(data.boneData.size());
        for (size_t i = 0; i < data.boneData.size(); ++i) {
            auto index = static_cast<BoneIndex>(i);

            skeleton->bones[i] = std::make_unique<Bone>();
            skeleton->bones[i]->index = index;
            skeleton->bones[i]->name = data.boneData[i].name;
            skeleton->bones[i]->parent = data.boneData[i].parentIndex;

            SKY_ASSERT(skeleton->nameToIndexMap.emplace(data.boneData[i].name, index).second);
        }
        skeleton->UpdateBoneTree();

        skeleton->refPos = std::make_shared<Pose>();
        skeleton->refPos->transforms = data.refPos;
        skeleton->refPos->skeleton = skeleton;
        return skeleton;
    }

    const Bone* Skeleton::GetBoneByName(const Name& name) const
    {
        auto iter = nameToIndexMap.find(name);
        return iter != nameToIndexMap.end() ? GetBoneByIndex(iter->second) : nullptr;
    }

} // namespace sky
