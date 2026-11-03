#pragma once
#include <animation/AnimationSourceTypes.h>
namespace sky {
    class IAnimationSkeletonSource {
    public:
        virtual uint64_t GetChangeSerial() const
        {
            return 0;
        }
        virtual ~IAnimationSkeletonSource()                                 = default;
        virtual AnimationSourceRequest<Skeleton> Request(const Uuid &asset) = 0;
    };
} // namespace sky
