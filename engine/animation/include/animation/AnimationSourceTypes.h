#pragma once

#include <animation/core/AnimationClip.h>
#include <animation/core/Skeleton.h>
#include <memory>

namespace sky {
    enum class AnimationSourceState : uint8_t { Pending, Ready, Error };

    struct AnimationAssetVersion {
        Uuid     asset;
        Uuid     generation;
        uint64_t revision                                        = 0;
        uint64_t layout                                          = 0;
        bool     operator==(const AnimationAssetVersion &) const = default;
    };

    template <class T>
    struct AnimationSourceResult {
        AnimationSourceResult()                                  = default;
        AnimationSourceResult(const AnimationSourceResult &)     = default;
        AnimationSourceResult(AnimationSourceResult &&) noexcept = default;
        AnimationSourceResult &operator=(AnimationSourceResult other) noexcept
        {
            using std::swap;
            swap(assetLease, other.assetLease);
            swap(state, other.state);
            swap(status, other.status);
            swap(version, other.version);
            swap(skeletonHint, other.skeletonHint);
            swap(resource, other.resource);
            return *this;
        }

        // Asset owners outlive the published resource. Consumers treat resources as immutable.
        std::shared_ptr<const void> assetLease;
        AnimationSourceState        state = AnimationSourceState::Pending;
        AnimationStatus             status;
        AnimationAssetVersion       version;
        Uuid                        skeletonHint;
        CounterPtr<T>               resource;
    };

    template <class T>
    class IAnimationSourceRequest {
    public:
        virtual ~IAnimationSourceRequest()      = default;
        virtual AnimationSourceResult<T> Poll() = 0;
    };

    template <class T>
    class AnimationSourceRequest {
    public:
        AnimationSourceRequest() = default;
        explicit AnimationSourceRequest(std::shared_ptr<IAnimationSourceRequest<T>> request) : operation(std::move(request))
        {
        }
        AnimationSourceRequest(const AnimationSourceRequest &)     = default;
        AnimationSourceRequest(AnimationSourceRequest &&) noexcept = default;
        AnimationSourceRequest &operator=(AnimationSourceRequest other) noexcept
        {
            operation.swap(other.operation);
            return *this;
        }
        explicit operator bool() const
        {
            return operation != nullptr;
        }
        AnimationSourceResult<T> Poll() const
        {
            if (*this) {
                return operation->Poll();
            }
            AnimationSourceResult<T> result;
            result.state  = AnimationSourceState::Error;
            result.status = {AnimationError::InvalidAsset, "Animation asset source is unavailable"};
            return result;
        }

    private:
        std::shared_ptr<IAnimationSourceRequest<T>> operation;
    };
} // namespace sky
