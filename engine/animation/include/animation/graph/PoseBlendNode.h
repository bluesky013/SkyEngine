//
// Created by blues on 2025/12/14.
//

#pragma once

#include <animation/core/AnimationUtils.h>
#include <animation/graph/AnimationNode.h>

namespace sky {

    class PoseBlendNodeList : public AnimNode {
    public:
        PoseBlendNodeList() = default;
        ~PoseBlendNodeList() override = default;

        void AddPose(AnimNode* node, float weight);
        void SetPoseWeight(size_t index, float weight);

        FORCEINLINE size_t GetPoseCount() const { return poses.size(); }
        FORCEINLINE AnimNode* GetPose(size_t index) const { return poses[index]; }
        FORCEINLINE float GetPoseWeight(size_t index) const { return desiredWeights[index]; }

        void InitAny(const AnimContext& context) override {}
        void EvalAny(AnimationEval& context) override;

        bool LowerToPlan(AnimationPlanBuilder& builder, const AnimPlanLowerInfo& info, uint32_t& outSlot) override;

    private:
        std::vector<AnimNode*> poses;
        std::vector<float> desiredWeights;
    };

    class PoseBlend2Node : public AnimNode {
    public:
        PoseBlend2Node(AnimNode* a, AnimNode* b);
        ~PoseBlend2Node() override = default;

        void SetBlend(bool enable) { blendEnable = enable; }
        void SetBlendTime(float time) { fadeInOut.SetBlendTime(time); }
        void SetAdditive(bool enable) { additive = enable; }

        FORCEINLINE bool IsAdditive() const { return additive; }
        FORCEINLINE AnimNode* GetPoseA() const { return poseA; }
        FORCEINLINE AnimNode* GetPoseB() const { return poseB; }
        FORCEINLINE float GetBlendedAlpha() const { return blendedAlpha; }

        void UpdateBlendControl(float deltaTime);

        void InitAny(const AnimContext& context) override;
        void TickAny(const AnimLayerContext& context, float deltaTime) override;
        void EvalAny(AnimationEval& context) override;

        bool LowerToPlan(AnimationPlanBuilder& builder, const AnimPlanLowerInfo& info, uint32_t& outSlot) override;

    private:
        AnimNode* poseA;
        AnimNode* poseB;

        AnimFadeInOut fadeInOut;

        float blendedAlpha = 0.f;

        bool blendEnable = false;
        bool additive = false;
        bool isARelevant = false;
        bool isBRelevant = false;
    };

} // namespace sky
