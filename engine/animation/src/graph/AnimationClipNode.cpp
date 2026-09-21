//
// Created by blues on 2026/1/1.
//

#include <animation/graph/AnimationClipNode.h>
#include <animation/core/AnimationClip.h>
#include <animation/core/AnimationTrackData.h>
#include <animation/core/AnimationUtils.h>
#include <animation/core/Skeleton.h>
#include <animation/plan/AnimationPlan.h>

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

    void AnimationClipNode::InitControl()
    {
        player.SetClip(data.clip);
        player.SetLoop(data.looping);
        player.SetPlaying(data.playing);
    }

    void AnimationClipNode::AdvanceControl(float deltaTime)
    {
        if (player.IsPlaying()) {
            player.Tick(deltaTime);
        }
    }

    void AnimationClipNode::InitAny(const AnimContext& context)
    {
        InitControl();
        player.SetPlaying(true);
    }

    void AnimationClipNode::TickAny(const AnimLayerContext& context, float deltaTime)
    {
        AdvanceControl(deltaTime);
    }

    void AnimationClipNode::EvalAny(AnimationEval& context)
    {
        if (!data.clip) {
            return;
        }

        SampleParam param = {};
        param.frameTime = Anim::ConvertFromFrameRate(player.GetCurrentTime(), data.clip->GetPlayRate());
        param.interpolation = AnimInterpolation::LINEAR;

        data.clip->SamplePose(context.pose, param);

        Skeleton* skeleton = context.pose.skeleton;
        if (skeleton != nullptr) {
            const auto& roots = skeleton->GetRoots();
            if (!roots.empty()) {
                const BoneIndex rootIndex = roots.front()->index;
                if (rootIndex < context.pose.transforms.size()) {
                    const Transform& refTrans = skeleton->GetRefPos()->transforms[rootIndex];
                    const Transform& sampled = context.pose.transforms[rootIndex];

                    context.rootMotionDelta = refTrans.GetInverse() * sampled;

                    if (!data.rootMotion) {
                        context.pose.transforms[rootIndex] = refTrans;
                    }
                }
            }
        }

        context.pose.NormalizeRotation();
    }

    bool AnimationClipNode::LowerToPlan(AnimationPlanBuilder& builder, const AnimPlanLowerInfo& info, uint32_t& outSlot)
    {
        if (!data.clip || info.skeleton == nullptr) {
            builder.AddError("clip node has no clip or skeleton");
            return false;
        }

        AnimationTrackData trackData;
        data.clip->BuildTrackData(trackData, *info.skeleton);

        const float frameRate = data.clip->GetPlayRate();
        const uint32_t clipIndex = builder.AddClip(trackData, frameRate);
        const uint32_t timeSlot = builder.AddTimeSlot();
        const uint32_t rootMotionSlot = builder.AddWeightSlot();
        const uint32_t maskIndex = builder.AddBoneMask(boneMask);
        const uint32_t slot = builder.AllocateSlot();

        AnimOpRecord op;
        op.op = AnimOp::ClipSample;
        op.output = static_cast<uint16_t>(slot);
        op.timeIndex = static_cast<uint16_t>(timeSlot);
        op.weightIndex = static_cast<uint16_t>(rootMotionSlot);
        op.boneMaskIndex = static_cast<uint16_t>(maskIndex);
        op.dataIndex = clipIndex;
        op.weight = frameRate;
        builder.EmitOp(op);

        builder.AddClipBinding(AnimationClipBinding{this, timeSlot, rootMotionSlot});
        outSlot = slot;
        return true;
    }

} // namespace sky