//
// Created by Zach Lee on 2025/6/3.
//

#pragma once

#include <animation/core/AnimationClip.h>
#include <animation/core/AnimationPose.h>
#include <core/math/Vector3.h>
#include <vector>
#include <memory>

namespace sky {

    // Motion feature represents the characteristics of a pose at a specific frame
    struct MotionFeature {
        // Joint positions in local space (relative to root)
        std::vector<Vector3> jointPositions;
        // Joint velocities
        std::vector<Vector3> jointVelocities;
        // Root velocity
        Vector3 rootVelocity;
        // Future trajectory positions (e.g., 0.2s, 0.4s, 0.6s ahead)
        std::vector<Vector3> futureTrajectory;
    };

    // Motion frame represents a single frame in the database
    struct MotionFrame {
        AnimClipPtr clip;           // Animation clip this frame belongs to
        uint32_t frameIndex;        // Frame index in the clip
        float timeInSeconds;        // Time in seconds
        MotionFeature feature;      // Features for this frame
        
        MotionFrame() : frameIndex(0), timeInSeconds(0.f) {}
    };

    // Motion database stores all motion data and features for matching
    class MotionDatabase {
    public:
        MotionDatabase() = default;
        ~MotionDatabase() = default;

        // Add an animation clip to the database
        void AddClip(const AnimClipPtr& clip, uint32_t sampleRate = 30);
        
        // Clear all data
        void Clear();
        
        // Get all motion frames
        const std::vector<MotionFrame>& GetFrames() const { return frames; }
        
        // Get number of frames
        size_t GetNumFrames() const { return frames.size(); }
        
        // Get a specific frame
        const MotionFrame* GetFrame(size_t index) const;

    private:
        // Extract features from animation clip
        void ExtractFeatures(const AnimClipPtr& clip, uint32_t sampleRate);
        
        // All motion frames in the database
        std::vector<MotionFrame> frames;
    };

    using MotionDatabasePtr = std::shared_ptr<MotionDatabase>;

} // namespace sky
