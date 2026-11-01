#pragma once

#include "OHOSInput.h"
#include <framework/window/NativeWindow.h>

namespace sky {

    class OHOSPlatform;

    class OHOSWindow final : public NativeWindow {
    public:
        explicit OHOSWindow(OHOSPlatform *platform);
        ~OHOSWindow() override;
        bool                        Init(const Descriptor &desc) override;
        std::shared_ptr<const void> RetainNativeHandle() const override;
        float                       GetDpiScale() const override;
        bool                        IsMainWindow() const override;
        void                        SetSurface(std::shared_ptr<const void> handle, uint32_t width, uint32_t height, float scale, bool notify);
        void                        DetachPlatform();
        void                        Touch(OHOSTouchAction action, int32_t identity, int32_t x, int32_t y);
        void                        Mouse(int32_t x, int32_t y, uint32_t buttons, float scroll);
        void                        Key(ScanCode code, bool down, KeyModFlags modifiers);
        void                        Text(std::u16string_view text);
        void                        TextKey(ScanCode code, uint32_t count);
        void                        CancelInput();
        void                        SetTextInputActive(bool active);

    private:
        void                        DispatchMouse(const std::vector<OHOSMouseChange> &changes);
        OHOSPlatform               *owner;
        std::shared_ptr<const void> surface;
        OHOSPointerState            pointerState;
        OHOSKeyState                keyState;
        uint32_t                    pendingTextDeletes = 0;
        bool                        textInputActive    = false;
    };

} // namespace sky
