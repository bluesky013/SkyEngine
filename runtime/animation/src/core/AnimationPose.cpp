//
// Created by blues on 2025/12/14.
//

#include <animation/core/AnimationPose.h>
#include <animation/core/Skeleton.h>
#include <animation/core/AnimationInterpolation.h>
#include <core/logger/Logger.h>

static const char* TAG = "Animation";

namespace sky {

    static void UpdateBone(const Bone* bone, const AnimPose& pose, Matrix4* outMatrix, const Transform &world) // NOLINT
    {
        SKY_ASSERT(bone != nullptr);

        const auto &trans = pose.transforms[bone->index];
        Transform current = world * trans;

        outMatrix[bone->index] = current.ToMatrix();
        for (const auto& childIdx : bone->children) {
            const auto* child = pose.skeleton->GetBoneByIndex(childIdx);
            assert(child->index == childIdx);
            UpdateBone(child, pose, outMatrix, current);
        }
    }

    void AnimPose::ResetRefPose()
    {
        if (skeleton != nullptr) {
            *this = *skeleton->GetRefPos();
        }
    }

    void AnimPose::NormalizeRotation()
    {
        for (auto& trans : transforms) {
            trans.rotation.Normalize();
        }
    }

    void AnimPose::ToSkinRenderData(std::vector<Matrix4>& skinUpdateData, const Transform& rootMatrix) const
    {
        const auto& roots = skeleton->GetRoots();
        skinUpdateData.resize(skeleton->GetNumBones());
        for (const auto& root : roots) {
            UpdateBone(root, *this, skinUpdateData.data(), rootMatrix);
        }
    }

    void AnimPose::BlendTransform(const Transform& src, Transform& dst, float weight)
    {
        dst.translation = dst.translation * weight;
        dst.scale = dst.scale * weight;
        dst.rotation = dst.rotation * weight;
    }

    void AnimPose::BlendTransformAdditive(const Transform& delta, Transform& dst, float weight)
    {
        dst.translation += delta.translation * weight;
        dst.scale += delta.scale * weight;
        dst.rotation = AccumulateShortest(dst.rotation, delta.rotation * weight);
    }

    void AnimPose::BlendPose(const AnimPose & src, AnimPose & dst, float weight, PoseBlendMode mode)
    {
        if (src.transforms.size() != dst.transforms.size()) {
            LOG_E(TAG, "Invalid Blend Pose size not match src[%llu], dst[%llu]", src.transforms.size(), dst.transforms.size());
            return;
        }

        if (mode == PoseBlendMode::ADDITIVE) {
            for (uint32_t index = 0; index < src.transforms.size(); ++index) {
                BlendTransformAdditive(src.transforms[index], dst.transforms[index], weight);
            }
        } else {
            for (uint32_t index = 0; index < src.transforms.size(); ++index) {
                BlendTransform(src.transforms[index], dst.transforms[index], weight);
            }
        }
    }

} // namespace sky