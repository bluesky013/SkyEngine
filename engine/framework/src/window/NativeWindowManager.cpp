//
// Created by blues on 2024/6/14.
//

#include <framework/window/NativeWindowManager.h>

namespace sky {

    void NativeWindowManager::Register(NativeWindow *window)
    {
        windowLut.emplace(window->GetWinId(), window);
    }

    void NativeWindowManager::UnRegister(NativeWindow *window)
    {
        windowLut.erase(window->GetWinId());
    }

    NativeWindow *NativeWindowManager::GetWindowByID(WindowID id) const
    {
        auto iter = windowLut.find(id);
        return iter == windowLut.end() ? nullptr : iter->second;
    }

    NativeWindow *NativeWindowManager::GetMainWindow() const
    {
        for (const auto &[id, window] : windowLut) {
            if (window != nullptr && window->IsMainWindow()) {
                return window;
            }
        }
        return nullptr;
    }
} // namespace sky
