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

        skeleton->bones.resize(data.boneData.size());
        for (size_t i = 0; i < data.boneData.size(); ++i) {
            BoneIndex index = static_cast<BoneIndex>(i);

            skeleton->bones[i] = std::make_unique<Bone>();
            skeleton->bones[i]->index = index;
            skeleton->bones[i]->parent = data.boneData[i].parentIndex;
            skeleton->bones[i]->skeleton = skeleton;
            SKY_ASSERT(skeleton->nameToIndexMap.emplace(data.boneData[i].name, index).second);
        }
        skeleton->UpdateBoneTree();

        return skeleton;
    }

} // namespace sky
