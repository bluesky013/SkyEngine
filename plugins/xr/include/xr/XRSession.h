//
// Created by blues on 2024/3/3.
//
#pragma once

#include <openxr/openxr.h>
#include <xr/XRCore.h>
#include <vector>

namespace sky {
    class XRInstance;

    class XRSession {
    public:
        explicit XRSession(XRInstance &inst) : instance(inst) {}
        ~XRSession();

        bool Init(const void *graphicsBinding);
        void SessionStateHandler(const XrEventDataSessionStateChanged& stateChangedEvent);
        void PollActions();

        XrSpace GetSpace() const { return space; }
        XrSession GetHandle() const { return session; }
        XRInstance &GetInstance() const { return instance; }
        bool IsRunning() const { return running; }

        void BeginFrame();
        void EndFrame(const XrFrameEndInfo &endInfo);

        const XrFrameState &FrameState() const { return frameState; }

    private:
        XRInstance &instance;
        XrSession session = XR_NULL_HANDLE;
        XrSpace space = XR_NULL_HANDLE;
        XrSessionState sessionState{XR_SESSION_STATE_UNKNOWN};
        bool running = false;
        XRInputState inputState;
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
    };

} // namespace sky