//
// Created by Zach Lee on 2025/6/3.
//

#include <animation/motion/MotionDatabase.h>
#include <animation/core/AnimationUtils.h>
#include <animation/core/Skeleton.h>

namespace sky {

    void MotionDatabase::AddClip(const AnimClipPtr& clip, uint32_t sampleRate)
    {
        if (!clip) {
            return;
        }
        
        ExtractFeatures(clip, sampleRate);
    }

    void MotionDatabase::Clear()
    {
        frames.clear();
    }

    const MotionFrame* MotionDatabase::GetFrame(size_t index) const
    {
        if (index >= frames.size()) {
            return nullptr;
        }
        return &frames[index];
    }

    void MotionDatabase::ExtractFeatures(const AnimClipPtr& clip, uint32_t sampleRate)
    {
        if (!clip) {
            return;
        }
        
        float duration = clip->GetDuration();
        float playRate = clip->GetPlayRate();
        
        if (duration <= 0.f || playRate <= 0.f) {
            return;
        }
        
        // Calculate number of samples based on sample rate
        uint32_t numSamples = static_cast<uint32_t>(duration * static_cast<float>(sampleRate));
        float timeStep = 1.0f / static_cast<float>(sampleRate);
        
        // Sample the animation at regular intervals
        for (uint32_t i = 0; i < numSamples; ++i) {
            float time = static_cast<float>(i) * timeStep;
            
            MotionFrame frame;
            frame.clip = clip;
            frame.frameIndex = i;
            frame.timeInSeconds = time;
            
            // Note: Feature extraction would require sampling the animation clip
            // and computing velocities and trajectories. This is a simplified version.
            // In a full implementation, you would:
            // 1. Sample pose at current time
            // 2. Sample pose at previous/next frames to compute velocities
            // 3. Project future trajectory based on motion
            // 4. Store joint positions relative to root
            
            frames.push_back(frame);
        }
    }

} // namespace sky
