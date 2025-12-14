//
// Created by blues on 2025/12/14.
//

#include <animation/skeleton/Pose.h>
#include <animation/skeleton/Skeleton.h>
#include <animation/core/AnimationInterpolation.h>
#include <core/logger/Logger.h>

static const char* TAG = "Animation";

namespace sky {

    void Pose::ResetRefPose()
    {
        if (skeleton != nullptr) {
            *this = *skeleton->GetRefPos();
        }
    }

    void Pose::BlendTransform(const Transform& src, Transform& dst, float weight)
    {
        dst.translation = dst.translation * weight;
        dst.scale = dst.scale * weight;
        dst.rotation = dst.rotation * weight;
    }

    void Pose::BlendTransformAdditive(const Transform& delta, Transform& dst, float weight)
    {
        dst.translation += delta.translation * weight;
        dst.scale += delta.scale * weight;
        dst.rotation = AccumulateShortest(dst.rotation, delta.rotation * weight);
    }

    void Pose::BlendPose(const Pose& src, Pose& dst, float weight, PoseBlendMode mode)
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