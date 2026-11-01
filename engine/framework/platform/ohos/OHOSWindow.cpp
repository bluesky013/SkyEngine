#include "OHOSWindow.h"
#include "OHOSPlatform.h"

#include <framework/window/NativeWindowManager.h>

namespace sky {

    OHOSWindow::OHOSWindow(OHOSPlatform *platform) : owner(platform)
    {
    }

    OHOSWindow::~OHOSWindow()
    {
        CancelInput();
        if (owner != nullptr) {
            owner->UnbindWindow(this);
        }
    }

    bool OHOSWindow::Init(const Descriptor &desc)
    {
        if (owner == nullptr || !owner->IsOwnerThread() || (desc.handle != nullptr && desc.handle != owner->GetMainWinHandle())) {
            return false;
        }
        descriptor = desc;
        if (!owner->BindWindow(this)) {
            return false;
        }
        SetID(static_cast<WindowID>(reinterpret_cast<uintptr_t>(this)));
        NativeWindowManager::Get()->Register(this);
        return true;
    }

    std::shared_ptr<const void> OHOSWindow::RetainNativeHandle() const
    {
        return surface;
    }
    float OHOSWindow::GetDpiScale() const
    {
        return scale;
    }
    bool OHOSWindow::IsMainWindow() const
    {
        return true;
    }

    void OHOSWindow::SetSurface(std::shared_ptr<const void> handle, uint32_t width, uint32_t height, float dpi, bool notify)
    {
        const bool replaced = handle.get() != surface.get();
        const bool resized  = width != descriptor.width || height != descriptor.height;
        if (replaced || resized || width == 0 || height == 0) {
            CancelInput();
        }
        auto previous     = std::move(surface);
        surface           = std::move(handle);
        winHandle         = const_cast<void *>(surface.get());
        descriptor.width  = width;
        descriptor.height = height;
        scale             = dpi;
        if (notify && replaced) {
            Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnSurfaceChanged, static_cast<const NativeWindow *>(this));
        }
        if (notify && resized) {
            const WindowResizeEvent event = {winID, width, height};
            Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnWindowResize, event);
        }
    }

    void OHOSWindow::DetachPlatform()
    {
        owner = nullptr;
        SetSurface({}, 0, 0, scale, false);
    }

    void OHOSWindow::DispatchMouse(const std::vector<OHOSMouseChange> &changes)
    {
        for (const auto &change : changes) {
            switch (change.type) {
            case OHOSMouseChange::Type::MOTION: {
                const MouseMotionEvent event = {winID, change.x, change.y, change.relX, change.relY};
                Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseMotion, event);
                break;
            }
            case OHOSMouseChange::Type::DOWN:
            case OHOSMouseChange::Type::UP: {
                const MouseButtonEvent event = {winID, change.button, 1, change.x, change.y};
                if (change.type == OHOSMouseChange::Type::DOWN) {
                    Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonDown, event);
                } else {
                    Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonUp, event);
                }
                break;
            }
            case OHOSMouseChange::Type::WHEEL: {
                const MouseWheelEvent event = {winID, change.x, change.y, change.scroll};
                Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseWheel, event);
                break;
            }
            case OHOSMouseChange::Type::END: break;
            }
        }
    }

    void OHOSWindow::Touch(OHOSTouchAction action, int32_t identity, int32_t x, int32_t y)
    {
        DispatchMouse(pointerState.Touch(action, identity, x, y));
    }

    void OHOSWindow::Mouse(int32_t x, int32_t y, uint32_t buttons, float scroll)
    {
        DispatchMouse(pointerState.Mouse(x, y, buttons, scroll));
    }

    void OHOSWindow::Key(ScanCode code, bool down, KeyModFlags modifiers)
    {
        if (!keyState.Apply(code, down, modifiers)) {
            return;
        }
        if (down && textInputActive && (code == ScanCode::KEY_BACKSPACE || code == ScanCode::KEY_DELETE)) {
            ++pendingTextDeletes;
        }
        const KeyboardEvent event = {winID, code, modifiers};
        Event<IKeyboardEvent>::BroadCast(down ? &IKeyboardEvent::OnKeyDown : &IKeyboardEvent::OnKeyUp, event);
    }

    void OHOSWindow::Text(std::u16string_view text)
    {
        if (textInputActive) {
            const auto committed = OHOSCommittedText(text);
            if (!committed.empty()) {
                Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnTextInput, winID, committed.c_str());
            }
            pendingTextDeletes = 0;
        }
    }

    void OHOSWindow::TextKey(ScanCode code, uint32_t count)
    {
        if (!textInputActive) {
            return;
        }
        if (code == ScanCode::KEY_BACKSPACE || code == ScanCode::KEY_DELETE) {
            const uint32_t duplicate = std::min(pendingTextDeletes, count);
            pendingTextDeletes -= duplicate;
            count -= duplicate;
        }
        const KeyboardEvent event = {winID, code, {}};
        for (uint32_t index = 0; index < count; ++index) {
            Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyDown, event);
            Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyUp, event);
        }
    }

    void OHOSWindow::CancelInput()
    {
        DispatchMouse(pointerState.Cancel());
        for (auto event : keyState.Cancel()) {
            event.winID = winID;
            Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyUp, event);
        }
        pendingTextDeletes = 0;
    }

    void OHOSWindow::SetTextInputActive(bool active)
    {
        textInputActive    = active;
        pendingTextDeletes = 0;
    }

    NativeWindow *NativeWindow::Create(const Descriptor &desc)
    {
        auto *implementation = Platform::Get()->GetImpl();
        if (implementation == nullptr || implementation->GetType() != PlatformType::HarmonyOS) {
            return nullptr;
        }
        auto window = std::make_unique<OHOSWindow>(static_cast<OHOSPlatform *>(implementation));
        return window->Init(desc) ? window.release() : nullptr;
    }

} // namespace sky
