#pragma once

#include <animation/core/AnimationClip.h>
#include <animation/core/Skeleton.h>
#include <animation/graph/GraphDefinition.h>
#include <memory>
#include <unordered_map>

namespace sky::animation {
    struct GraphNodeDebug {
        Uuid   node;
        float  sampleTime = 0.f;
        double elapsed    = 0.0;
        bool   active     = false;
    };
    struct GraphDebugState {
        Uuid                        activeState;
        Uuid                        destinationState;
        Uuid                        transition;
        float                       progress = 0.f;
        bool                        playing  = false;
        std::vector<GraphNodeDebug> nodes;
    };
    class GraphInstance {
    public:
        using Clips = std::unordered_map<Uuid, AnimClipPtr>;
        static std::unique_ptr<GraphInstance>
        Create(const CookedGraph &graph, const SkeletonPtr &skeleton, const Clips &clips, AnimationStatus &status);
        ~GraphInstance();
        GraphInstance(const GraphInstance &)                                = delete;
        GraphInstance                     &operator=(const GraphInstance &) = delete;
        AnimationStatus                    Tick(float delta);
        void                               Evaluate(AnimPose &out);
        AnimationStatus                    SetParameter(const Uuid &parameter, const AnimationParam &value);
        const std::vector<AnimationParam> &GetParameters() const;
        AnimationStatus                    SetRate(float rate);
        void                               SetPlaying(bool value);
        void                               Stop();
        bool                               IsPlaying() const;
        GraphDebugState                    Debug() const;

    private:
        GraphInstance();
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
} // namespace sky::animation
