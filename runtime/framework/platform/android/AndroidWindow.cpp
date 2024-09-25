//
// Created by Zach Lee on 2022/9/26.
//

#include "AndroidWindow.h"
#include <android/native_window.h>
#include <framework/platform/PlatformBase.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "core/event/Event.h"
#include <framework/window/IWindowEvent.h>

namespace sky {

    bool AndroidWindow::Init(const Descriptor &desc)
    {
        descriptor = desc;
        if (descriptor.nativeHandle != nullptr) {
            winHandle = descriptor.nativeHandle;
            descriptor.width = ANativeWindow_getWidth(static_cast<ANativeWindow*>(winHandle));
            descriptor.height = ANativeWindow_getHeight(static_cast<ANativeWindow*>(winHandle));
        }
        return winHandle != nullptr;
    }

    void AndroidWindow::PollEvent(bool &quit)
    {
        android_app *app = static_cast<android_app*>(Platform::Get()->GetNativeApp());
//        android_input_buffer* inputBuffer = android_app_swap_input_buffers(app);
//
//        if (inputBuffer == nullptr) {
//            return;
//        }
//
//        if (inputBuffer->keyEventsCount != 0) {
//            for (uint64_t i = 0; i < inputBuffer->keyEventsCount; ++i) {
//                GameActivityKeyEvent *keyEvent = &inputBuffer->keyEvents[i];
//                // process key event
//            }
//            android_app_clear_key_events(inputBuffer);
//        }
//
//
//        if (inputBuffer && inputBuffer->motionEventsCount) {
//            for (uint64_t i = 0; i < inputBuffer->motionEventsCount; ++i) {
//                GameActivityMotionEvent* motionEvent = &inputBuffer->motionEvents[i];
//
//                if (motionEvent->pointerCount > 0) {
//                    const int action = motionEvent->action;
//                    const int actionMasked = action & AMOTION_EVENT_ACTION_MASK;
//                    const int button = motionEvent->buttonState;
//
//                    auto mouseX = static_cast<int32_t>(GameActivityPointerAxes_getX(&motionEvent->pointers[0]));
//                    auto mouseY = static_cast<int32_t>(GameActivityPointerAxes_getY(&motionEvent->pointers[0]));
//
//                    switch (button) {
//                        case AMOTION_EVENT_BUTTON_PRIMARY:
//                            break;
//                        case AMOTION_EVENT_BUTTON_SECONDARY:
//                            break;
//                        case AMOTION_EVENT_BUTTON_TERTIARY:
//                            break;
//                        case AMOTION_EVENT_BUTTON_BACK:
//                            break;
//                        case AMOTION_EVENT_BUTTON_FORWARD:
//                            break;
//                        default:
//                            break;
//                    }
//
//                    if (actionMasked == AMOTION_EVENT_ACTION_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
//                        Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnMouseButtonDown, 1);
//                    } else if (actionMasked == AMOTION_EVENT_ACTION_UP || actionMasked == AMOTION_EVENT_ACTION_POINTER_UP) {
//                        Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnMouseButtonUp, 1);
//                    } else if (actionMasked == AMOTION_EVENT_ACTION_SCROLL) {
//                        Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnMouseWheel, mouseX, mouseY);
//                    } else if (actionMasked == AMOTION_EVENT_ACTION_MOVE) {
//                        Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnMouseMove, mouseX, mouseY);
//                    }
//                }
//            }
//            android_app_clear_motion_events(inputBuffer);
//        }
    }

    NativeWindow *NativeWindow::Create(const Descriptor &des)
    {
        NativeWindow *window = new AndroidWindow();
        if (!window->Init(des)) {
            delete window;
            window = nullptr;
        }
        return window;
    }
}
