//
// Created by zach on 2024/3/9.
//

#pragma once

#include <core/platform/Platform.h>
#include <core/logger/logger.h>
#include <openxr/openxr.h>
#include <array>

#define XR_FAILED(result) ((result) < 0)

#define XR_CHECK_RESULT(result) \
    {                           \
        if (XR_FAILED(result)) {\
            LOG_E("XR", "error result: %d", result); \
            SKY_UNEXPECTED;     \
        }                       \
    }

namespace sky {

    enum class XRSide : uint32_t {
        LEFT = 0,
        RIGHT = 1,
        NUM
    };


    struct XRInputState {
        XrActionSet actionSet{XR_NULL_HANDLE};
        XrAction grabAction{XR_NULL_HANDLE};
        XrAction poseAction{XR_NULL_HANDLE};
        XrAction vibrateAction{XR_NULL_HANDLE};
        XrAction quitAction{XR_NULL_HANDLE};
        std::array<XrPath, static_cast<uint32_t>(XRSide::NUM)> handSubActionPath;
        std::array<XrSpace, static_cast<uint32_t>(XRSide::NUM)> handSpace;
        std::array<float, static_cast<uint32_t>(XRSide::NUM)> handScale = {{1.0f, 1.0f}};
        std::array<XrBool32, static_cast<uint32_t>(XRSide::NUM)> handActive;
    };

} // namespace sky