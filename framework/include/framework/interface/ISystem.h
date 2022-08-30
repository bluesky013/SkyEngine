//
// Created by Zach Lee on 2022/3/12.
//

#pragma once

namespace sky {
    class SettingRegistry;

    class ISystemNotify {
    public:
        ISystemNotify()          = default;
        virtual ~ISystemNotify() = default;

        virtual void SetExit() = 0;

        virtual const SettingRegistry &GetSettings() const = 0;

        virtual const NativeWindow *GetViewport() const = 0;
    };

} // namespace sky