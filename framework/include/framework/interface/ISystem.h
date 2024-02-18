//
// Created by Zach Lee on 2022/3/12.
//

#pragma once

namespace sky {
    class SettingRegistry;
    class NativeWindow;
    class ModuleManager;

    class ISystemNotify {
    public:
        ISystemNotify()          = default;
        virtual ~ISystemNotify() = default;

        virtual void SetExit() = 0;

        virtual const NativeWindow *GetViewport() const { return nullptr; }
        virtual ModuleManager* GetModuleManager() const { return nullptr; }
    };

} // namespace sky
