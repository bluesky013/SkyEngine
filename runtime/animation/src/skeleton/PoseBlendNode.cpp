//
// Created by blues on 2025/12/14.
//

#include <animation/skeleton/PoseBlendNode.h>
#include <animation/skeleton/Skeleton.h>

namespace sky {

    void PoseBlendNode::Eval(PoseContext& context, float deltaTime)
    {
        // quick check return
        if (poses.size() != cachedWeights.size()) {
            return;
        }

        std::vector<PoseContext> tmpCtx;
        tmpCtx.reserve(poses.size());

        for (uint32_t index = 0; index < poses.size(); ++index) {
            const float weight = cachedWeights[index];
            if (weight <= BLEND_WEIGHT_THRESHOLD) {
                continue;
            }

            tmpCtx.emplace_back(context);
            PoseContext &ctx = tmpCtx.back();
            poses[index]->Eval(ctx, deltaTime);
        }

        // override output
        context.pose.ResetRefPose();

        for (uint32_t i = 0; i < poses.size(); ++i) {
            Pose::BlendPose(tmpCtx[i].pose, context.pose, cachedWeights[i], PoseBlendMode::ADDITIVE);
        }
    }

} // namespace sky