//
// Created by blues on 2026/1/1.
//

#include <animation/graph/AnimationClipNode.h>
#include <animation/core/AnimationUtils.h>

namespace sky {

    AnimationClipNode::AnimationClipNode(const AnimClipPtr& inClip)
        : clip(inClip)
    {
    }

    void AnimationClipNode::SetLooping(bool loop)
    {
        looping = loop;
    }

    void AnimationClipNode::SetRootMotion(bool enable)
    {
        rootMotion = enable;
    }

    void AnimationClipNode::InitAny(const AnimContext& context)
    {
        player.SetClip(clip);
        player.SetLoop(looping);
        player.SetPlaying(false);
    }

    void AnimationClipNode::TickAny(const AnimLayerContext& context, float deltaTime)
    {
        player.Tick(deltaTime);
    }

    void AnimationClipNode::EvalAny(PoseContext& context)
    {
        if (!clip) {
            return;
        }

        SampleParam param = {};
        param.frameTime = Anim::ConvertFromFrameRate(player.GetCurrentTime(), clip->GetPlayRate());
        param.interpolation = AnimInterpolation::LINEAR;

        clip->SamplePose(context.pose, param);
        context.pose.NormalizeRotation();
    }

} // namespace sky