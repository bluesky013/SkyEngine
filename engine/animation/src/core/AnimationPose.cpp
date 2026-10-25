//
// Created by blues on 2025/12/14.
//

#include <animation/core/AnimationInterpolation.h>
#include <animation/core/AnimationPose.h>
#include <animation/core/Skeleton.h>
#include <array>
#include <core/logger/Logger.h>

static const char *TAG = "Animation";

namespace sky {

    void AnimPose::ResetRefPose()
    {
        if (skeleton != nullptr) {
            *this = *skeleton->GetRefPos();
        }
    }

    void AnimPose::NormalizeRotation()
    {
        for (auto &trans : transforms) {
            trans.rotation.Normalize();
        }
    }

    AnimationStatus AnimPose::ToSkinRenderData(std::vector<Matrix4> &skinUpdateData, const Transform &rootMatrix) const
    {
        if (skeleton == nullptr || transforms.size() != skeleton->GetNumBones()) {
            return {AnimationError::InvalidSkeleton, "model matrix generation requires a complete pose"};
        }
        const auto &roots = skeleton->GetRoots();
        skinUpdateData.resize(skeleton->GetNumBones());
        std::array<BoneIndex, ANIM_MAX_BONE> pending{};
        size_t                               count = 0;
        for (const auto &root : roots) {
            pending[count++] = root->index;
        }
        const Matrix4 world = rootMatrix.ToMatrix();
        while (count != 0) {
            const auto *bone   = skeleton->GetBoneByIndex(pending[--count]);
            const auto &parent = bone->parent == INVALID_BONE_ID ? world : skinUpdateData[bone->parent];
            // Matrix composition preserves nonuniform parent scale across rotated children.
            skinUpdateData[bone->index] = parent * transforms[bone->index].ToMatrix();
            for (const auto child : bone->children) {
                pending[count++] = child;
            }
        }
        return {};
    }

    void AnimPose::BlendTransform(const Transform &src, Transform &dst, float weight)
    {
        dst.translation = dst.translation * (1.f - weight) + src.translation * weight;
        dst.scale       = dst.scale * (1.f - weight) + src.scale * weight;
        dst.rotation    = AnimSphericalLinear(dst.rotation, src.rotation, weight);
    }

    void AnimPose::BlendTransformAdditive(const Transform &delta, Transform &dst, float weight)
    {
        dst.translation += delta.translation * weight;
        dst.scale += delta.scale * weight;

        const Quaternion weightedDelta = AnimSphericalLinear({}, delta.rotation, weight);
        dst.rotation                   = dst.rotation * weightedDelta;
        dst.rotation.Normalize();
    }

    void AnimPose::BlendPose(const AnimPose &src, AnimPose &dst, float weight, PoseBlendMode mode)
    {
        if (src.transforms.size() != dst.transforms.size()) {
            LOG_E(TAG, "Invalid Blend Pose size not match src[%zu], dst[%zu]", src.transforms.size(), dst.transforms.size());
            return;
        }

        if (mode == PoseBlendMode::ADDITIVE) {
            for (size_t index = 0; index < src.transforms.size(); ++index) {
                BlendTransformAdditive(src.transforms[index], dst.transforms[index], weight);
            }
        } else {
            for (size_t index = 0; index < src.transforms.size(); ++index) {
                BlendTransform(src.transforms[index], dst.transforms[index], weight);
            }
        }
    }

} // namespace sky
