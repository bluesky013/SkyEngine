//
// Created by blues on 2026/1/1.
//

#pragma once

#include <animation/graph/AnimationNode.h>
#include <animation/core/AnimationPlayer.h>

namespace sky {

    class AnimationClipNode : public AnimNode {
    public:
        struct PersistentData {
            AnimClipPtr clip;
            bool looping = false;
            bool rootMotion = false;
        };

        struct Data : PersistentData {
            bool playing = false;
        };

        explicit AnimationClipNode(const PersistentData& inData);
        ~AnimationClipNode() override = default;

        void SetPlaying(bool play);
        void SetLooping(bool loop);
        void SetEnableRootMotion(bool enable);

        FORCEINLINE bool IsPlaying() const { return player.IsPlaying(); }
        FORCEINLINE bool IsLooping() const { return data.looping; }
        FORCEINLINE bool IsRootMotionEnable() const { return data.rootMotion; }

        void PreTick(const AnimationTick& tick) override;

        void InitAny(const AnimContext& context) override;
        void TickAny(const AnimLayerContext& context, float deltaTime) override;
        void EvalAny(AnimationEval& context) override;

    private:
        // status
        Data data;

        // async data
        AnimationSequencePlayer player;
    };

} // namespace sky
