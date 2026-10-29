//
// Created on 2026/10/15.
//

#pragma once

#include <cstdint>
#include <framework/window/IWindowEvent.h>
#include <string>
#include <string_view>
#include <vector>

namespace sky {
    enum class EventPollMode : uint8_t;

    // Kept independent of the NDK so lifecycle and input semantics are tested on the host.
    enum class AndroidLifecycleEvent { RESUME, PAUSE, FOCUS_GAINED, FOCUS_LOST, SURFACE_AVAILABLE, SURFACE_LOST, DESTROY };

    class AndroidLifecycleState {
    public:
        void     Apply(AndroidLifecycleEvent event);
        bool     IsFrameActive() const;
        bool     HasFocus() const;
        bool     IsDestroyed() const;
        uint64_t GetEpoch() const;
        int      GetPollTimeout(EventPollMode mode) const;

    private:
        bool     resumed   = false;
        bool     focused   = false;
        bool     surface   = false;
        bool     destroyed = false;
        uint64_t epoch     = 0;
    };

    enum class AndroidMotionAction { DOWN, UP, MOVE, POINTER_DOWN, POINTER_UP, CANCEL, HOVER, SCROLL, BUTTON };

    struct AndroidPointerPosition {
        int32_t id = 0;
        int32_t x  = 0;
        int32_t y  = 0;
    };

    struct AndroidMotionSnapshot {
        AndroidMotionAction                 action      = AndroidMotionAction::MOVE;
        uint32_t                            actionIndex = 0;
        bool                                touch       = true;
        uint32_t                            buttons     = 0; // Android PRIMARY, SECONDARY, TERTIARY bits (1, 2, 4).
        float                               scroll      = 0.f;
        std::vector<AndroidPointerPosition> pointers;
    };

    struct AndroidMouseChange {
        enum class Type { MOTION, DOWN, UP, WHEEL };
        Type            type   = Type::MOTION;
        MouseButtonType button = MouseButtonType::LEFT;
        int32_t         x      = 0;
        int32_t         y      = 0;
        int32_t         relX   = 0;
        int32_t         relY   = 0;
        float           scroll = 0.f;
    };

    class AndroidPointerState {
    public:
        std::vector<AndroidMouseChange> Apply(const AndroidMotionSnapshot &motion);
        std::vector<AndroidMouseChange> Cancel();

    private:
        void     UpdateButtons(uint32_t next, std::vector<AndroidMouseChange> &changes);
        int32_t  primary    = -1;
        int32_t  x          = 0;
        int32_t  y          = 0;
        bool     positioned = false;
        uint32_t buttons    = 0;
    };

    ScanCode    AndroidKeyCodeToScanCode(int32_t keyCode);
    KeyModFlags AndroidMetaStateToModifiers(int32_t metaState);

    struct AndroidTextEdit {
        std::string inserted;
        uint32_t    deleted = 0;
    };

    // GameTextInput reports a complete editable buffer in JNI modified UTF-8.
    // Emit only committed edits, convert surrogate pairs, and suppress repeated snapshots.
    class AndroidTextState {
    public:
        AndroidTextEdit Apply(std::string_view modifiedUtf8, bool composing);
        void            Reset();

    private:
        std::string committed;
    };

} // namespace sky
