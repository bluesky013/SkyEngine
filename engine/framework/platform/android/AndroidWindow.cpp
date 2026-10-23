//
// Created by Zach Lee on 2022/9/26.
//

#include "AndroidWindow.h"
#include "AndroidPlatform.h"

#include <algorithm>
#include <android/configuration.h>
#include <android/native_window.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindowManager.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace sky {

    AndroidWindow::~AndroidWindow()
    {
        CancelInput();
        auto *platform = static_cast<AndroidPlatform *>(Platform::Get()->GetImpl());
        if (platform != nullptr && platform->GetMainWindow() == this) {
            platform->SetNativeWindow(nullptr);
        }
        // NativeWindow's destructor unregisters the stable engine window ID.
    }

    bool AndroidWindow::Init(const Descriptor &desc)
    {
        descriptor = desc;
        winHandle  = descriptor.handle != nullptr ? descriptor.handle : Platform::Get()->GetMainWinHandle();
        if (winHandle == nullptr) {
            return false;
        }
        SetID(static_cast<WindowID>(reinterpret_cast<uintptr_t>(this)));
        NativeWindowManager::Get()->Register(this);
        const auto *app = static_cast<android_app *>(Platform::Get()->GetNativeApp());
        UpdateMetrics(app->config);
        return true;
    }

    std::shared_ptr<const void> AndroidWindow::RetainNativeHandle() const
    {
        auto *handle = static_cast<ANativeWindow *>(winHandle);
        if (!handle)
            return {};
        ANativeWindow_acquire(handle);
        return {handle, [](const void *value) { ANativeWindow_release(static_cast<ANativeWindow *>(const_cast<void *>(value))); }};
    }

    void AndroidWindow::SetWinHandle(void *handle)
    {
        if (handle == nullptr) {
            CancelInput();
        }
        winHandle = handle;
        // The identity remains stable when Android destroys/recreates the surface.
        Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnSurfaceChanged, static_cast<const NativeWindow *>(this));
    }

    void AndroidWindow::UpdateMetrics(AConfiguration *configuration)
    {
        const int density = configuration != nullptr ? AConfiguration_getDensity(configuration) : ACONFIGURATION_DENSITY_DEFAULT;
        scale             = density > 0 && density < ACONFIGURATION_DENSITY_ANY ? static_cast<float>(density) / 160.f : 1.f;
        if (winHandle == nullptr) {
            return;
        }
        auto     *handle = static_cast<ANativeWindow *>(winHandle);
        const int width  = ANativeWindow_getWidth(handle);
        const int height = ANativeWindow_getHeight(handle);
        if (width <= 0 || height <= 0) {
            return;
        }
        if (descriptor.width != static_cast<uint32_t>(width) || descriptor.height != static_cast<uint32_t>(height)) {
            descriptor.width              = static_cast<uint32_t>(width);
            descriptor.height             = static_cast<uint32_t>(height);
            const WindowResizeEvent event = {winID, descriptor.width, descriptor.height};
            Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnWindowResize, event);
        }
    }

    void AndroidWindow::DispatchMouse(const std::vector<AndroidMouseChange> &changes)
    {
        for (const auto &change : changes) {
            switch (change.type) {
            case AndroidMouseChange::Type::MOTION: {
                const MouseMotionEvent event = {winID, change.x, change.y, change.relX, change.relY};
                Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseMotion, event);
                break;
            }
            case AndroidMouseChange::Type::DOWN:
            case AndroidMouseChange::Type::UP: {
                const MouseButtonEvent event = {winID, change.button, 1, change.x, change.y};
                if (change.type == AndroidMouseChange::Type::DOWN) {
                    Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonDown, event);
                } else {
                    Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonUp, event);
                }
                break;
            }
            case AndroidMouseChange::Type::WHEEL: {
                const MouseWheelEvent event = {winID, change.x, change.y, change.scroll};
                Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseWheel, event);
                break;
            }
            }
        }
    }

    void AndroidWindow::CancelInput()
    {
        DispatchMouse(pointerState.Cancel());
        for (auto event : pressedKeys) {
            event.mod = {};
            Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyUp, event);
        }
        pressedKeys.clear();
        pendingTextDeletes = 0;
    }

    void AndroidWindow::ResetTextInput()
    {
        textState.Reset();
        textInputActive    = true;
        pendingTextDeletes = 0;
    }

    void AndroidWindow::EndTextInput()
    {
        textInputActive    = false;
        pendingTextDeletes = 0;
        textState.Reset();
    }

    void AndroidWindow::ProcessText(const GameTextInputState &state)
    {
        if (!textInputActive || state.text_UTF8 == nullptr || state.text_length < 0) {
            return;
        }
        const bool          composing    = state.composingRegion.start >= 0 && state.composingRegion.end > state.composingRegion.start;
        const auto          edit         = textState.Apply(std::string_view(state.text_UTF8, static_cast<size_t>(state.text_length)), composing);
        const KeyboardEvent backspace    = {winID, ScanCode::KEY_BACKSPACE, {}};
        const uint32_t      deletedByIme = edit.deleted - std::min(edit.deleted, pendingTextDeletes);
        if (!composing) {
            pendingTextDeletes = 0;
        }
        for (uint32_t index = 0; index < deletedByIme; ++index) {
            Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyDown, backspace);
            Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyUp, backspace);
        }
        if (!edit.inserted.empty()) {
            Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnTextInput, winID, edit.inserted.c_str());
        }
    }

    void AndroidWindow::PollInput(android_app *app, bool deliver)
    {
        auto *input = android_app_swap_input_buffers(app);
        if (input != nullptr) {
            if (deliver) {
                for (uint64_t index = 0; index < input->keyEventsCount; ++index) {
                    const auto    &key      = input->keyEvents[index];
                    const ScanCode scanCode = AndroidKeyCodeToScanCode(key.keyCode);
                    if (scanCode == ScanCode::KEY_NUM) {
                        continue;
                    }
                    const KeyboardEvent event = {winID, scanCode, AndroidMetaStateToModifiers(key.metaState)};
                    const auto          iter  = std::find_if(pressedKeys.begin(), pressedKeys.end(),
                                                             [scanCode](const KeyboardEvent &value) { return value.scanCode == scanCode; });
                    if (key.action == AKEY_EVENT_ACTION_DOWN) {
                        if (textInputActive && (scanCode == ScanCode::KEY_BACKSPACE || scanCode == ScanCode::KEY_DELETE)) {
                            ++pendingTextDeletes;
                        }
                        if (iter == pressedKeys.end()) {
                            pressedKeys.push_back(event);
                        }
                        Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyDown, event);
                    } else if (key.action == AKEY_EVENT_ACTION_UP) {
                        if (iter != pressedKeys.end()) {
                            pressedKeys.erase(iter);
                        }
                        Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyUp, event);
                    }
                }
                for (uint64_t index = 0; index < input->motionEventsCount; ++index) {
                    auto                 &motion = input->motionEvents[index];
                    AndroidMotionSnapshot snapshot;
                    const int             action = motion.action & AMOTION_EVENT_ACTION_MASK;
                    switch (action) {
                    case AMOTION_EVENT_ACTION_DOWN: snapshot.action = AndroidMotionAction::DOWN; break;
                    case AMOTION_EVENT_ACTION_UP: snapshot.action = AndroidMotionAction::UP; break;
                    case AMOTION_EVENT_ACTION_MOVE: snapshot.action = AndroidMotionAction::MOVE; break;
                    case AMOTION_EVENT_ACTION_POINTER_DOWN: snapshot.action = AndroidMotionAction::POINTER_DOWN; break;
                    case AMOTION_EVENT_ACTION_POINTER_UP: snapshot.action = AndroidMotionAction::POINTER_UP; break;
                    case AMOTION_EVENT_ACTION_CANCEL: snapshot.action = AndroidMotionAction::CANCEL; break;
                    case AMOTION_EVENT_ACTION_HOVER_MOVE: snapshot.action = AndroidMotionAction::HOVER; break;
                    case AMOTION_EVENT_ACTION_SCROLL: snapshot.action = AndroidMotionAction::SCROLL; break;
                    case AMOTION_EVENT_ACTION_BUTTON_PRESS:
                    case AMOTION_EVENT_ACTION_BUTTON_RELEASE: snapshot.action = AndroidMotionAction::BUTTON; break;
                    default: continue;
                    }
                    snapshot.actionIndex = (motion.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                    snapshot.touch = (motion.source & AINPUT_SOURCE_MOUSE) != AINPUT_SOURCE_MOUSE && snapshot.action != AndroidMotionAction::HOVER &&
                                     snapshot.action != AndroidMotionAction::SCROLL && snapshot.action != AndroidMotionAction::BUTTON;
                    snapshot.buttons     = static_cast<uint32_t>(motion.buttonState);
                    const uint32_t count = std::min(motion.pointerCount, static_cast<uint32_t>(GAMEACTIVITY_MAX_NUM_POINTERS_IN_MOTION_EVENT));
                    for (uint32_t pointerIndex = 0; pointerIndex < count; ++pointerIndex) {
                        const auto &pointer = motion.pointers[pointerIndex];
                        snapshot.pointers.push_back({pointer.id, static_cast<int32_t>(GameActivityPointerAxes_getX(&pointer)),
                                                     static_cast<int32_t>(GameActivityPointerAxes_getY(&pointer))});
                    }
                    if (count != 0) {
                        snapshot.scroll = GameActivityPointerAxes_getAxisValue(&motion.pointers[0], AMOTION_EVENT_AXIS_VSCROLL);
                    }
                    DispatchMouse(pointerState.Apply(snapshot));
                }
            }
            android_app_clear_key_events(input);
            android_app_clear_motion_events(input);
        }
        pthread_mutex_lock(&app->mutex);
        const bool textPending = app->textInputState != 0;
        app->textInputState    = 0;
        pthread_mutex_unlock(&app->mutex);
        if (textPending && deliver && textInputActive) {
            GameActivity_getTextInputState(
                app->activity, [](void *context, const GameTextInputState *state) { static_cast<AndroidWindow *>(context)->ProcessText(*state); },
                this);
        }
    }

    NativeWindow *NativeWindow::Create(const Descriptor &descriptor)
    {
        auto *platform = static_cast<AndroidPlatform *>(Platform::Get()->GetImpl());
        if (platform->GetMainWindow() != nullptr) {
            return nullptr; // Android owns one main surface per activity.
        }
        NativeWindow *window = new AndroidWindow();
        if (!window->Init(descriptor)) {
            delete window;
            return nullptr;
        }
        platform->SetNativeWindow(static_cast<AndroidWindow *>(window));
        return window;
    }

} // namespace sky
