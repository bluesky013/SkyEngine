//
// Created by Zach Lee on 2022/9/26.
//

#pragma once

#include "AndroidInput.h"
#include <framework/window/NativeWindow.h>

struct android_app;
struct AConfiguration;
struct GameTextInputState;

namespace sky {

    class AndroidWindow : public NativeWindow {
    public:
        AndroidWindow() = default;
        ~AndroidWindow() override;

        // The OS window handle may be (re)created on resume; keep the engine
        // window bound to the live ANativeWindow.
        void SetWinHandle(void *handle);
        void UpdateMetrics(AConfiguration *configuration);
        void PollInput(android_app *app, bool deliver);
        void CancelInput();
        void ResetTextInput();
        void EndTextInput();
        void ProcessText(const GameTextInputState &state);

        // Called on WINDOW_RESIZED / CONFIG_CHANGED so GetWidth/GetHeight report
        // the live surface size.
        float GetDpiScale() const override
        {
            return scale;
        }
        bool IsMainWindow() const override
        {
            return true;
        }

    private:
        bool                       Init(const Descriptor &desc) override;
        void                       DispatchMouse(const std::vector<AndroidMouseChange> &changes);
        AndroidPointerState        pointerState;
        AndroidTextState           textState;
        std::vector<KeyboardEvent> pressedKeys;
        bool                       textInputActive    = false;
        uint32_t                   pendingTextDeletes = 0;
    };

} // namespace sky
