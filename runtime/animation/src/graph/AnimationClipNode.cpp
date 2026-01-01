//
// Created by blues on 2026/1/1.
//

#include <animation/graph/AnimationClipNode.h>

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

        clip->SamplePose(context.pose, player.GetCurrentTime());
        context.pose.NormalizeRotation();
    }

} // namespace sky