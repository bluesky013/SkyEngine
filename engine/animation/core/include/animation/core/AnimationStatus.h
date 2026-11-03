#pragma once

#include <cstdint>
#include <string>

namespace sky {

    enum class AnimationError : uint8_t {
        None,
        InvalidSkeleton,
        InvalidChannel,
        InvalidDuration,
        InvalidPlayback,
        MissingClip,
        UnsupportedVersion,
        InvalidAsset,
        MissingProvider,
        StaleGeneration,
        Unsupported,
    };

    struct AnimationStatus {
        AnimationError error = AnimationError::None;
        std::string    message;

        explicit operator bool() const
        {
            return error == AnimationError::None;
        }
    };

} // namespace sky
