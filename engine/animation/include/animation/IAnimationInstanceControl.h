#pragma once

#include <animation/AnimationSourceTypes.h>
#include <variant>

namespace sky {
    class World;

    enum class AnimationMode : uint8_t { Clip, Graph };
    enum class AnimationRootPolicy : uint8_t { KeepInPose, LockToReference };
    enum class AnimationControlOp : uint8_t { Play, Pause, Stop, Seek, SetRate, Reload };

    struct AnimationInstanceHandle {
        const World *world = nullptr;
        Uuid         worldEpoch;
        Uuid         actor;
        Uuid         generation;
        bool         operator==(const AnimationInstanceHandle &) const = default;
    };

    struct AnimationParameterState {
        Uuid           id;
        std::string    name;
        AnimationParam value;
    };
    struct AnimationNodePlayback {
        Uuid   node;
        float  time    = 0.f;
        double elapsed = 0.0;
        bool   active  = false;
    };
    struct AnimationInstanceStatus {
        Uuid                                 activeState;
        Uuid                                 destinationState;
        Uuid                                 transition;
        float                                transitionProgress = 0.f;
        std::vector<AnimationParameterState> parameters;
        std::vector<AnimationNodePlayback>   nodes;

        AnimationInstanceHandle handle;
        AnimationSourceState    state = AnimationSourceState::Pending;
        AnimationStatus         diagnostic;
        AnimationAssetVersion   skeleton;
        AnimationAssetVersion   animation;
        uint64_t                poseRevision    = 0;
        float                   time            = 0.f;
        float                   duration        = 0.f;
        float                   rate            = 1.f;
        bool                    playing         = false;
        bool                    retainedVersion = false;
    };

    class IAnimationInstanceControl {
    public:
        virtual ~IAnimationInstanceControl() = default;
        // Optional authoring capability: a registered component with a reflected UUID member "skeleton".
        virtual Uuid GetComponentType() const
        {
            return {};
        }
        virtual std::string_view GetWorldSystemName() const
        {
            return {};
        }
        // Main-thread calls only. Handles become stale after detach, reset, or binding changes.
        virtual AnimationInstanceStatus Find(const World *world, const Uuid &actor) const                                                         = 0;
        virtual AnimationStatus         Control(const AnimationInstanceHandle &instance, AnimationControlOp op, float value = 0.f)                = 0;
        virtual AnimationStatus         SetParameter(const AnimationInstanceHandle &instance, const Uuid &parameter, const AnimationParam &value) = 0;
    };
} // namespace sky
