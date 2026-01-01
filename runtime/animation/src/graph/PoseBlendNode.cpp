//
// Created by blues on 2025/12/14.
//

#include <animation/graph/PoseBlendNode.h>
#include <animation/core/Skeleton.h>
#include <algorithm>

namespace sky {

    void PoseBlendNodeList::EvalAny(PoseContext& context)
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
            poses[index]->EvalAny(ctx);
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

    void PoseBlend2Node::InitAny(const AnimContext& context)
    {
        AnimNode::InitAny(context);

        poseA->InitAny(context);
        poseB->InitAny(context);

        isARelevant = false;
        isBRelevant = false;

        fadeInOut.Reset();
    }

    void PoseBlend2Node::TickAny(const AnimLayerContext& context, float deltaTime)
    {
        blendedAlpha = fadeInOut.Eval(deltaTime, blendEnable);
        blendedAlpha = std::clamp(blendedAlpha, 0.f, 1.f);

        const bool tmpARelevant = !Anim::IsFullWeight(blendedAlpha);
        const bool tmpBRelevant = Anim::IsRelevant(blendedAlpha);

        if (tmpARelevant && !isARelevant) {
            poseA->InitAny(context);
        }

        if (tmpBRelevant && !isBRelevant) {
            poseB->InitAny(context);
        }

        isARelevant = tmpARelevant;
        isBRelevant = tmpBRelevant;

        if (isBRelevant) {

            if (isARelevant) {
                poseA->TickAny(context.MakeContext(1.f - blendedAlpha), deltaTime);
                poseB->TickAny(context.MakeContext(blendedAlpha), deltaTime);
            } else {
                poseB->TickAny(context, deltaTime);
            }
        } else {
            poseA->TickAny(context, deltaTime);
        }
    }

    void PoseBlend2Node::EvalAny(PoseContext& context)
    {
        if (isBRelevant) {

            if (isARelevant) {

                PoseContext pose1(context);
                poseA->EvalAny(pose1);

                PoseContext pose2(context);
                poseB->EvalAny(pose2);

                AnimPose::BlendPose(pose1.pose, context.pose, 1.f - blendedAlpha, PoseBlendMode::OVERRIDE);
                AnimPose::BlendPose(pose2.pose, context.pose, blendedAlpha, PoseBlendMode::ADDITIVE);
                context.pose.NormalizeRotation();

            } else {
                poseB->EvalAny(context);
            }

        } else {
            poseA->EvalAny(context);
        }

    }

} // namespace sky