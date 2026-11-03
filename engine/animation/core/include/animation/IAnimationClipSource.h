#pragma once
#include <animation/AnimationSourceTypes.h>
namespace sky {
    // Request and Poll are main-thread operations and never wait for cook or asset workers.
    class IAnimationClipSource {
    public:
        virtual uint64_t GetChangeSerial() const
        {
            return 0;
        }
        virtual ~IAnimationClipSource()                                          = default;
        virtual AnimationSourceRequest<AnimationClip> Request(const Uuid &asset) = 0;
    };
} // namespace sky
