//
// Created by blues on 2026/1/1.
//

#include <animation/core/AnimationClip.h>
#include <animation/core/AnimationTrackData.h>
#include <animation/core/AnimationUtils.h>
#include <animation/core/Skeleton.h>
#include <animation/graph/AnimationClipNode.h>
#include <animation/plan/AnimationPlan.h>

namespace sky {

    AnimationClipNode::AnimationClipNode(const PersistentData &inData) : data(inData)
    {
    }

    void AnimationClipNode::SetPlaying(bool play)
    {
        data.playing = play;
        if (initialized) {
            status = player.SetPlaying(play);
        }
    }

    void AnimationClipNode::SetLooping(bool loop)
    {
        data.looping = loop;
        player.SetLoop(loop);
    }

    void AnimationClipNode::SetEnableRootMotion(bool enable)
    {
        data.rootMotion = enable;
    }

    void AnimationClipNode::PreTick(const AnimationTick &tick)
    {
        player.SetLoop(data.looping);
    }

    AnimationStatus AnimationClipNode::SetPlayRate(float rate)
    {
        status = player.SetPlayRate(rate);
        if (status) {
            data.playRate = rate;
        }
        return status;
    }

    AnimationStatus AnimationClipNode::Seek(float time)
    {
        status = player.Seek(time);
        return status;
    }

    void AnimationClipNode::Stop()
    {
        data.playing = false;
        player.Stop();
    }

    void AnimationClipNode::InitControl()
    {
        initialized = false;
        status      = player.SetClip(data.clip);
        if (!status) {
            return;
        }
        player.SetLoop(data.looping);
        status = player.SetPlayRate(data.playRate);
        if (!status) {
            return;
        }
        status      = player.SetPlaying(data.playing);
        initialized = static_cast<bool>(status);
    }

    void AnimationClipNode::AdvanceControl(float deltaTime)
    {
        status = player.Tick(deltaTime);
    }

    void AnimationClipNode::InitAny(const AnimContext &context)
    {
        InitControl();
    }

    void AnimationClipNode::TickAny(const AnimLayerContext &context, float deltaTime)
    {
        AdvanceControl(deltaTime);
    }

    void AnimationClipNode::EvalAny(AnimationEval &context)
    {
        if (!data.clip) {
            status = {AnimationError::MissingClip, "clip node has no clip"};
            return;
        }

        SampleParam param   = {};
        param.frameTime     = Anim::ConvertFromFrameRate(player.GetCurrentTime(), data.clip->GetPlayRate());
        param.interpolation = AnimInterpolation::LINEAR;

        status = data.clip->SamplePose(context.pose, param);
        if (!status) {
            return;
        }

        Skeleton *skeleton = context.pose.skeleton;
        if (skeleton != nullptr) {
            const auto &roots = skeleton->GetRoots();
            if (!roots.empty()) {
                const BoneIndex rootIndex = roots.front()->index;
                if (rootIndex < context.pose.transforms.size()) {
                    const Transform &refTrans = skeleton->GetRefPos()->transforms[rootIndex];
                    const Transform &sampled  = context.pose.transforms[rootIndex];

                    context.rootMotionDelta = refTrans.GetInverse() * sampled;

                    if (!data.rootMotion) {
                        context.pose.transforms[rootIndex] = refTrans;
                    }
                }
            }
        }

        context.pose.NormalizeRotation();
    }

    bool AnimationClipNode::LowerToPlan(AnimationPlanBuilder &builder, const AnimPlanLowerInfo &info, uint32_t &outSlot)
    {
        if (!data.clip || info.skeleton == nullptr) {
            builder.AddError("clip node has no clip or skeleton");
            return false;
        }

        AnimationTrackData trackData;
        status = data.clip->BuildTrackData(trackData, *info.skeleton);
        if (!status) {
            builder.AddError(status.message);
            return false;
        }

        const float    frameRate      = data.clip->GetPlayRate();
        const uint32_t clipIndex      = builder.AddClip(trackData, frameRate);
        const uint32_t timeSlot       = builder.AddTimeSlot();
        const uint32_t rootMotionSlot = builder.AddWeightSlot();
        const uint32_t maskIndex      = builder.AddBoneMask(boneMask);
        const uint32_t slot           = builder.AllocateSlot();

        AnimOpRecord op;
        op.op            = AnimOp::ClipSample;
        op.output        = static_cast<uint16_t>(slot);
        op.timeIndex     = static_cast<uint16_t>(timeSlot);
        op.weightIndex   = static_cast<uint16_t>(rootMotionSlot);
        op.boneMaskIndex = static_cast<uint16_t>(maskIndex);
        op.dataIndex     = clipIndex;
        op.weight        = frameRate;
        builder.EmitOp(op);

        builder.AddClipBinding(AnimationClipBinding{this, timeSlot, rootMotionSlot});
        outSlot = slot;
        return true;
    }

} // namespace sky
