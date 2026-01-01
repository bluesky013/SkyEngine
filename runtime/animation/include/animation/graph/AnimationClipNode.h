//
// Created by blues on 2026/1/1.
//

#pragma once

#include <animation/graph/AnimationNode.h>
#include <animation/core/AnimationPlayer.h>

namespace sky {

    class AnimationClipNode : public AnimNode {
    public:
        explicit AnimationClipNode(const AnimClipPtr& inClip);
        ~AnimationClipNode() override = default;

        void SetLooping(bool loop);
        void SetRootMotion(bool enable);

        void InitAny(const AnimContext& context) override;
        void TickAny(const AnimLayerContext& context, float deltaTime) override;
        void EvalAny(PoseContext& context) override;

    private:
        // res
        AnimClipPtr clip;

        // state
        bool looping = false;
        bool rootMotion = false;

        // async data
        AnimationSequencePlayer player;
    };

} // namespace sky
