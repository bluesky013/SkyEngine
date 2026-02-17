//
// Created by Zach Lee on 2025/6/3.
//

#pragma once

#include <animation/graph/AnimationNode.h>
#include <animation/motion/MotionMacher.h>
#include <animation/motion/MotionDatabase.h>
#include <animation/core/AnimationPlayer.h>
#include <animation/core/AnimationUtils.h>

namespace sky {

    // Motion matching node provides advanced animation selection using motion matching
    class MotionMatchingNode : public AnimNode {
    public:
        // Persistent configuration data
        struct PersistentData {
            MotionDatabasePtr database;     // Motion database to search
            bool rootMotion = false;        // Enable root motion
            float blendTime = 0.2f;         // Transition blend time
            uint32_t searchInterval = 3;    // Frames between searches (optimization)
        };

        // Runtime data
        struct Data : PersistentData {
            size_t currentFrameIndex = 0;   // Current frame in database
            size_t targetFrameIndex = 0;    // Target frame for transition
            float currentTime = 0.f;        // Current playback time
            bool transitioning = false;     // Whether currently transitioning
            float transitionTime = 0.f;     // Time spent in transition
            uint32_t framesSinceSearch = 0; // Frames since last search
        };

        explicit MotionMatchingNode(const PersistentData& inData);
        ~MotionMatchingNode() override = default;

        // Set motion query parameters
        void SetDesiredVelocity(const Vector3& velocity);
        void SetDesiredTrajectory(const std::vector<Vector3>& trajectory);
        
        // Control
        void SetEnableRootMotion(bool enable);
        void SetBlendTime(float time);
        void SetSearchInterval(uint32_t interval);

        // AnimNode interface
        void PreTick(const AnimationTick& tick) override;
        void InitAny(const AnimContext& context) override;
        void TickAny(const AnimLayerContext& context, float deltaTime) override;
        void EvalAny(AnimationEval& context) override;

    private:
        // Perform motion matching search
        void SearchBestMatch();
        
        // Build motion query from current state
        MotionQuery BuildQuery() const;
        
        // Update animation playback
        void UpdatePlayback(float deltaTime);
        
        // Sample pose from current frame
        void SampleCurrentPose(AnimPose& pose);
        
        // Sample pose from target frame (for blending)
        void SampleTargetPose(AnimPose& pose);

        // Runtime data
        Data data;
        
        // Motion matcher
        MotionMatcher matcher;
        
        // Query parameters (updated by user)
        Vector3 desiredVelocity;
        std::vector<Vector3> desiredTrajectory;
        
        // Animation player for current clip
        AnimationSequencePlayer currentPlayer;
        
        // Animation player for target clip (during transition)
        AnimationSequencePlayer targetPlayer;
        
        // Blend helper for transitions
        AnimFadeInOut fadeInOut;
        
        // Cached poses for blending
        AnimFinalPose currentPose;
        AnimFinalPose targetPose;
    };

} // namespace sky
