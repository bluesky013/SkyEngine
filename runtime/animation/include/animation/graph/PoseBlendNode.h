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

        void InitAny(const AnimContext& context) override {}
        void EvalAny(AnimationEval& context) override;

    private:
        std::vector<AnimNode*> poses;
        std::vector<float> desiredWeights;
        std::vector<float> cachedWeights;
    };

    class PoseBlend2Node : public AnimNode {
    public:
        PoseBlend2Node(AnimNode* a, AnimNode* b);
        ~PoseBlend2Node() override = default;

        void InitAny(const AnimContext& context) override;
        void TickAny(const AnimLayerContext& context, float deltaTime) override;
        void EvalAny(AnimationEval& context) override;

    private:
        AnimNode* poseA;
        AnimNode* poseB;

        AnimFadeInOut fadeInOut;

        float blendedAlpha = 0.f;

        bool blendEnable = false;
        bool isARelevant = false;
        bool isBRelevant = false;
    };

} // namespace sky
