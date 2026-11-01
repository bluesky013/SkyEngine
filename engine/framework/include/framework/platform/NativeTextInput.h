#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace sky {
    struct NativeTextInputOptions {
        std::string text;
        std::string placeholder;
        std::string title;
        uint32_t    maxLength = 0; // Unicode scalar limit; zero means unlimited.
        bool        multiline = false;
        bool        password  = false;
    };

    enum class NativeTextInputEventType { CHANGED, CONFIRMED, CANCELLED, ERROR, END };

    struct NativeTextInputEvent {
        NativeTextInputEventType type = NativeTextInputEventType::CHANGED;
        std::string              text;
        int32_t                  nativeError = 0;
    };

    // CHANGED excludes preedit. One terminal event follows on the engine owner thread.
    using NativeTextInputCallback = std::function<void(const NativeTextInputEvent &)>;
} // namespace sky
