//
// Created by Zach Lee on 2025/6/3.
//

#include <animation/graph/MotionMatchingNode.h>
#include <animation/core/Skeleton.h>

namespace sky {

    MotionMatchingNode::MotionMatchingNode(const PersistentData& inData)
        : data(inData)
        , fadeInOut(inData.blendTime)
    {
        if (data.database) {
            matcher.SetDatabase(data.database);
        }
    }

    void MotionMatchingNode::SetDesiredVelocity(const Vector3& velocity)
    {
        desiredVelocity = velocity;
    }

    void MotionMatchingNode::SetDesiredTrajectory(const std::vector<Vector3>& trajectory)
    {
        desiredTrajectory = trajectory;
    }

    void MotionMatchingNode::SetEnableRootMotion(bool enable)
    {
        data.rootMotion = enable;
    }

    void MotionMatchingNode::SetBlendTime(float time)
    {
        data.blendTime = time;
        fadeInOut = AnimFadeInOut(time);
    }

    void MotionMatchingNode::SetSearchInterval(uint32_t interval)
    {
        data.searchInterval = interval;
    }

    void MotionMatchingNode::PreTick(const AnimationTick& tick)
    {
        // Optional: update based on tick if needed
    }

    void MotionMatchingNode::InitAny(const AnimContext& context)
    {
        if (!data.database || data.database->GetNumFrames() == 0) {
            return;
        }

        // Initialize with the first frame
        data.currentFrameIndex = 0;
        data.targetFrameIndex = 0;
        data.currentTime = 0.f;
        data.transitioning = false;
        data.transitionTime = 0.f;
        data.framesSinceSearch = 0;

        // Set up the initial clip
        const MotionFrame* frame = data.database->GetFrame(0);
        if (frame && frame->clip) {
            currentPlayer.SetClip(frame->clip);
            currentPlayer.SetLoop(true);
            currentPlayer.SetPlaying(true);
        }

        fadeInOut.Reset();
    }

    void MotionMatchingNode::TickAny(const AnimLayerContext& context, float deltaTime)
    {
        if (!data.database || data.database->GetNumFrames() == 0) {
            return;
        }

        // Perform motion matching search at intervals
        data.framesSinceSearch++;
        if (data.framesSinceSearch >= data.searchInterval) {
            SearchBestMatch();
            data.framesSinceSearch = 0;
        }

        // Update playback
        UpdatePlayback(deltaTime);
    }

    void MotionMatchingNode::EvalAny(AnimationEval& context)
    {
        if (!data.database || data.database->GetNumFrames() == 0) {
            return;
        }

        if (data.transitioning) {
            // Blend between current and target poses
            AnimationEval currentEval(context);
            AnimationEval targetEval(context);
            
            SampleCurrentPose(currentEval.pose);
            SampleTargetPose(targetEval.pose);
            
            // Calculate blend alpha based on transition progress
            float alpha = data.transitionTime / data.blendTime;
            alpha = std::clamp(alpha, 0.f, 1.f);
            
            // Blend poses - use OVERRIDE for both with weights
            context.pose.ResetRefPose();
            AnimPose::BlendPose(currentEval.pose, context.pose, 1.f - alpha, PoseBlendMode::OVERRIDE);
            AnimPose::BlendPose(targetEval.pose, context.pose, alpha, PoseBlendMode::OVERRIDE);
            context.pose.NormalizeRotation();
        } else {
            // Just sample the current pose
            SampleCurrentPose(context.pose);
        }

        // Handle root motion
        if (!data.rootMotion) {
            context.pose.boneMask.ResetBit(0);
        }
    }

    void MotionMatchingNode::SearchBestMatch()
    {
        MotionQuery query = BuildQuery();
        MotionMatchResult result = matcher.FindBestMatch(query);
        
        if (!result.valid) {
            return;
        }

        // If we found a better match, start transitioning to it
        if (result.frameIndex != data.currentFrameIndex) {
            data.targetFrameIndex = result.frameIndex;
            data.transitioning = true;
            data.transitionTime = 0.f;
            
            // Set up target player
            const MotionFrame* frame = data.database->GetFrame(result.frameIndex);
            if (frame && frame->clip) {
                targetPlayer.SetClip(frame->clip);
                targetPlayer.SetLoop(true);
                targetPlayer.SetPlaying(true);
            }
            
            fadeInOut.Reset();
        }
    }

    MotionQuery MotionMatchingNode::BuildQuery() const
    {
        MotionQuery query;
        
        // Set desired velocity and trajectory from user input
        query.desiredRootVelocity = desiredVelocity;
        query.desiredTrajectory = desiredTrajectory;
        
        // In a full implementation, we would extract current joint positions
        // and velocities from the current pose. For now, leave them empty.
        // This would require access to the current skeleton state.
        
        return query;
    }

    void MotionMatchingNode::UpdatePlayback(float deltaTime)
    {
        if (data.transitioning) {
            data.transitionTime += deltaTime;
            
            // Advance both players
            currentPlayer.Tick(deltaTime);
            targetPlayer.Tick(deltaTime);
            
            // Check if transition is complete
            if (data.transitionTime >= data.blendTime) {
                // Switch to target
                data.currentFrameIndex = data.targetFrameIndex;
                currentPlayer = targetPlayer;
                data.transitioning = false;
                data.transitionTime = 0.f;
            }
        } else {
            // Just advance the current player
            currentPlayer.Tick(deltaTime);
        }
        
        data.currentTime = currentPlayer.GetCurrentTime();
    }

    void MotionMatchingNode::SampleCurrentPose(AnimPose& pose)
    {
        const MotionFrame* frame = data.database->GetFrame(data.currentFrameIndex);
        if (!frame || !frame->clip) {
            return;
        }

        SampleParam param = {};
        param.frameTime = Anim::ConvertFromFrameRate(currentPlayer.GetCurrentTime(), frame->clip->GetPlayRate());
        param.interpolation = AnimInterpolation::LINEAR;

        frame->clip->SamplePose(pose, param);
    }

    void MotionMatchingNode::SampleTargetPose(AnimPose& pose)
    {
        const MotionFrame* frame = data.database->GetFrame(data.targetFrameIndex);
        if (!frame || !frame->clip) {
            return;
        }

        SampleParam param = {};
        param.frameTime = Anim::ConvertFromFrameRate(targetPlayer.GetCurrentTime(), frame->clip->GetPlayRate());
        param.interpolation = AnimInterpolation::LINEAR;

        frame->clip->SamplePose(pose, param);
    }

} // namespace sky
