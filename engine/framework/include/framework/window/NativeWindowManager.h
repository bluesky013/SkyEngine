//
// Created by blues on 2024/6/14.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/window/NativeWindow.h>

namespace sky {

    class NativeWindowManager : public Singleton<NativeWindowManager> {
    public:
        NativeWindowManager() = default;
        ~NativeWindowManager() override = default;

        void Register(NativeWindow *window);
        void UnRegister(NativeWindow *window);
        NativeWindow* GetWindowByID(WindowID id) const;
    private:
        std::unordered_map<WindowID, NativeWindow*> windowLut;
    };

} // namespace sky
