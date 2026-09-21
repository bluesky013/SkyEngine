//
// Created by blues on 2025/12/14.
//

#include <animation/graph/PoseBlendNode.h>
#include <animation/core/Skeleton.h>
#include <animation/plan/AnimationPlan.h>
#include <algorithm>

namespace sky {

    void PoseBlendNodeList::AddPose(AnimNode* node, float weight)
    {
        poses.emplace_back(node);
        desiredWeights.emplace_back(weight);
    }

    void PoseBlendNodeList::SetPoseWeight(size_t index, float weight)
    {
        if (index < desiredWeights.size()) {
            desiredWeights[index] = weight;
        }
    }

    void PoseBlendNodeList::EvalAny(AnimationEval& context)
    {
        if (poses.size() != desiredWeights.size()) {
            return;
        }

        context.pose.ResetRefPose();

        float accumulatedWeight = 0.f;
        for (size_t index = 0; index < poses.size(); ++index) {
            const float weight = desiredWeights[index];
            if (weight <= ANIM_BLEND_WEIGHT_THRESHOLD) {
                continue;
            }

            AnimationEval poseContext(context);
            poses[index]->EvalAny(poseContext);

            accumulatedWeight += weight;
            AnimPose::BlendPose(poseContext.pose, context.pose, weight / accumulatedWeight, PoseBlendMode::OVERRIDE);
        }

        context.pose.NormalizeRotation();
    }

    bool PoseBlendNodeList::LowerToPlan(AnimationPlanBuilder& builder, const AnimPlanLowerInfo& info, uint32_t& outSlot)
    {
        const size_t count = poses.size();
        if (count == 0 || count != desiredWeights.size()) {
            builder.AddError("pose list is empty or inconsistent");
            return false;
        }

        AnimationListBinding binding;
        binding.node = this;

        uint32_t accumulated = ANIM_INVALID_SLOT;

        for (size_t i = 0; i < count; ++i) {
            const float weight = desiredWeights[i];
            if (weight <= ANIM_BLEND_WEIGHT_THRESHOLD) {
                continue;
            }

            uint32_t child = ANIM_INVALID_SLOT;
            if (!poses[i]->LowerToPlan(builder, info, child)) {
                return false;
            }

            const uint32_t weightSlot = builder.AddWeightSlot();
            binding.poseIndices.push_back(static_cast<uint32_t>(i));
            binding.weightSlots.push_back(weightSlot);

            if (accumulated == ANIM_INVALID_SLOT) {
                accumulated = child;
                continue;
            }

            const uint32_t slot = builder.AllocateSlot();

            AnimOpRecord op;
            op.op = AnimOp::Blend;
            op.inputA = static_cast<uint16_t>(accumulated);
            op.inputB = static_cast<uint16_t>(child);
            op.output = static_cast<uint16_t>(slot);
            op.weightIndex = static_cast<uint16_t>(weightSlot);
            builder.EmitOp(op);

            accumulated = slot;
        }

        if (accumulated == ANIM_INVALID_SLOT) {
            builder.AddError("pose list has no active poses");
            return false;
        }

        builder.AddListBinding(binding);
        outSlot = accumulated;
        return true;
    }

    PoseBlend2Node::PoseBlend2Node(AnimNode* a, AnimNode* b)
        : poseA(a)
        , poseB(b)
        , fadeInOut(0.2f)
    {
    }

    void PoseBlend2Node::InitAny(const AnimContext& context)
    {
        poseA->InitAny(context);
        poseB->InitAny(context);

        isARelevant = false;
        isBRelevant = false;

        fadeInOut.Reset();
    }

    void PoseBlend2Node::UpdateBlendControl(float deltaTime)
    {
        blendedAlpha = std::clamp(fadeInOut.Eval(deltaTime, blendEnable), 0.f, 1.f);
    }

    void PoseBlend2Node::TickAny(const AnimLayerContext& context, float deltaTime)
    {
        UpdateBlendControl(deltaTime);

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

    void PoseBlend2Node::EvalAny(AnimationEval& context)
    {
        if (isBRelevant) {

            if (isARelevant) {

                AnimationEval pose1(context);
                poseA->EvalAny(pose1);

                AnimationEval pose2(context);
                poseB->EvalAny(pose2);

                context.pose.ResetRefPose();
                AnimPose::BlendPose(pose1.pose, context.pose, 1.f, PoseBlendMode::OVERRIDE);
                AnimPose::BlendPose(pose2.pose, context.pose, blendedAlpha, PoseBlendMode::OVERRIDE);
                context.pose.NormalizeRotation();

            } else {
                poseB->EvalAny(context);
            }

        } else {
            poseA->EvalAny(context);
        }

    }

    bool PoseBlend2Node::LowerToPlan(AnimationPlanBuilder& builder, const AnimPlanLowerInfo& info, uint32_t& outSlot)
    {
        uint32_t a = ANIM_INVALID_SLOT;
        uint32_t b = ANIM_INVALID_SLOT;
        if (!poseA->LowerToPlan(builder, info, a) || !poseB->LowerToPlan(builder, info, b)) {
            return false;
        }

        const uint32_t weightSlot = builder.AddWeightSlot();
        const uint32_t slot = builder.AllocateSlot();

        AnimOpRecord op;
        op.op = additive ? AnimOp::AdditiveBlend : AnimOp::Blend;
        op.inputA = static_cast<uint16_t>(a);
        op.inputB = static_cast<uint16_t>(b);
        op.output = static_cast<uint16_t>(slot);
        op.weightIndex = static_cast<uint16_t>(weightSlot);
        builder.EmitOp(op);

        builder.AddBlendBinding(AnimationBlendBinding{this, weightSlot});
        outSlot = slot;
        return true;
    }

} // namespace sky
