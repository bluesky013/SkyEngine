#pragma once

#include <animation/IAnimationInstanceControl.h>
#include <core/math/Matrix4.h>
#include <memory>

namespace sky {
    struct AnimationPoseBinding {
        AnimationInstanceHandle instance;
        AnimationAssetVersion   skeleton;
        bool                    operator==(const AnimationPoseBinding &) const = default;
    };

    struct AnimationPoseBase {
        AnimationPoseBinding binding;
        uint64_t             revision = 0;
        std::vector<Matrix4> models;
    };

    struct AnimationPoseContribution {
        AnimationStatus      status;
        AnimationPoseBinding binding;
        uint64_t             baseRevision = 0;
        uint64_t             revision     = 0;
        std::vector<Matrix4> models;
        // Optional complete per-bone model-space normal transforms, before inverse bind.
        std::vector<Matrix4> modelNormals;
    };

    class IAnimationPoseContributor {
    public:
        virtual ~IAnimationPoseContributor() = default;
        // Main-thread snapshot evaluation; results own their storage and never mutate the base.
        virtual AnimationPoseContribution Evaluate(const AnimationPoseBase &base) const = 0;
    };

    struct AnimationPoseContributionToken {
        Uuid                 id;
        AnimationPoseBinding binding;
    };

    class IAnimationPoseContributionRegistry {
    public:
        virtual ~IAnimationPoseContributionRegistry() = default;
        // One contribution slot per instance; stale bindings and occupied slots fail explicitly.
        virtual AnimationStatus RegisterPoseContributor(const AnimationPoseBinding                &binding,
                                                        std::shared_ptr<IAnimationPoseContributor> contributor,
                                                        AnimationPoseContributionToken            &out)           = 0;
        virtual AnimationStatus UnregisterPoseContributor(const AnimationPoseContributionToken &token) = 0;
    };
} // namespace sky
