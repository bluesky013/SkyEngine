//
// Created by blues on 2026/1/1.
//

#include <animation/graph/AnimationClipNode.h>
#include <animation/core/AnimationUtils.h>

namespace sky {

    AnimationClipNode::AnimationClipNode(const PersistentData& inData)
        : data(inData)
    {
    }

    void AnimationClipNode::SetPlaying(bool play)
    {
        data.playing = play;
    }

    void AnimationClipNode::SetLooping(bool loop)
    {
        data.looping = loop;
    }

    void AnimationClipNode::SetEnableRootMotion(bool enable)
    {
        data.rootMotion = enable;
    }

    void AnimationClipNode::PreTick(const AnimationTick& tick)
    {
        player.SetLoop(data.looping);
        player.SetPlaying(data.playing);
    }

    void AnimationClipNode::InitAny(const AnimContext& context)
    {
        player.SetClip(data.clip);
        player.SetLoop(data.looping);
        player.SetPlaying(false);
    }

    void AnimationClipNode::TickAny(const AnimLayerContext& context, float deltaTime)
    {
        if (player.IsPlaying()) {
            player.Tick(deltaTime);
        }
    }

    void AnimationClipNode::EvalAny(AnimationEval& context)
    {
        if (!data.clip) {
            return;
        }

        SampleParam param = {};
        param.frameTime = Anim::ConvertFromFrameRate(player.GetCurrentTime(), data.clip->GetPlayRate());
        param.interpolation = AnimInterpolation::LINEAR;

        if (!data.rootMotion) {
            context.pose.boneMask.ResetBit(0);
        }

        data.clip->SamplePose(context.pose, param);
        context.pose.NormalizeRotation();
    }

} // namespace sky