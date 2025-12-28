//
// Created by blues on 2025/12/14.
//

#include <animation/graph/PoseBlendNode.h>
#include <animation/core/Skeleton.h>
#include <algorithm>

namespace sky {

    void PoseBlendNodeList::EvalAsync(PoseContext& context, float deltaTime)
    {
        // quick check return
        if (poses.size() != cachedWeights.size()) {
            return;
        }

        std::vector<PoseContext> tmpCtx;
        tmpCtx.reserve(poses.size());

        for (uint32_t index = 0; index < poses.size(); ++index) {
            const float weight = cachedWeights[index];
            if (weight <= ANIM_BLEND_WEIGHT_THRESHOLD) {
                continue;
            }

            tmpCtx.emplace_back(context);
            PoseContext &ctx = tmpCtx.back();
            poses[index]->EvalAsync(ctx, deltaTime);
        }

        // override output
        context.pose.ResetRefPose();

        for (uint32_t i = 0; i < poses.size(); ++i) {
            AnimPose::BlendPose(tmpCtx[i].pose, context.pose, cachedWeights[i], PoseBlendMode::ADDITIVE);
        }
    }

    PoseBlend2Node::PoseBlend2Node(AnimNode* a, AnimNode* b)
        : poseA(a)
        , poseB(b)
        , fadeInOut(0.2f)
    {
    }

    void PoseBlend2Node::InitAsync(const AnimContext& context)
    {
        AnimNode::InitAsync(context);

        poseA->InitAsync(context);
        poseB->InitAsync(context);

        isARelevant = false;
        isBRelevant = false;

        fadeInOut.Reset();
    }

    void PoseBlend2Node::TickAsync(const AnimLayerContext& context, float deltaTime)
    {
        blendedAlpha = fadeInOut.Eval(deltaTime, blendEnable);
        blendedAlpha = std::clamp(blendedAlpha, 0.f, 1.f);

        const bool tmpARelevant = !Anim::IsFullWeight(blendedAlpha);
        const bool tmpBRelevant = Anim::IsRelevant(blendedAlpha);

        if (tmpARelevant && !isARelevant) {
            poseA->InitAsync(context);
        }

        if (tmpBRelevant && !isBRelevant) {
            poseB->InitAsync(context);
        }

        isARelevant = tmpARelevant;
        isBRelevant = tmpBRelevant;

        if (isBRelevant) {

            if (isARelevant) {
                poseA->TickAsync(context.MakeContext(1.f - blendedAlpha), deltaTime);
                poseB->TickAsync(context.MakeContext(blendedAlpha), deltaTime);
            } else {
                poseB->TickAsync(context, deltaTime);
            }
        } else {
            poseA->TickAsync(context, deltaTime);
        }
    }

    void PoseBlend2Node::EvalAsync(PoseContext& context, float deltaTime)
    {
        if (isBRelevant) {

            if (isARelevant) {

                PoseContext pose1(context);
                poseA->EvalAsync(pose1, deltaTime);

                PoseContext pose2(context);
                poseB->EvalAsync(pose2, deltaTime);

                AnimPose::BlendPose(pose1.pose, context.pose, 1.f - blendedAlpha, PoseBlendMode::OVERRIDE);
                AnimPose::BlendPose(pose2.pose, context.pose, blendedAlpha, PoseBlendMode::ADDITIVE);
                context.pose.NormalizeRotation();

            } else {
                poseB->EvalAsync(context, deltaTime);
            }

        } else {
            poseA->EvalAsync(context, deltaTime);
        }

    }

} // namespace sky