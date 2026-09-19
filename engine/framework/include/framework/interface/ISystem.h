//
// Created by Zach Lee on 2022/3/12.
//

#pragma once

#include <core/event/Event.h>

namespace sky {
    class NativeWindow;
    class ModuleManager;


    class ISystemNotify {
    public:
        ISystemNotify()          = default;
        virtual ~ISystemNotify() = default;
        virtual void SetExit() = 0;

        virtual ModuleManager* GetModuleManager() const { return nullptr; }

        // Native handle of the application's main window, used by render
        // modules to create a swapchain surface. Null when no window exists.
        virtual void *GetMainWindowHandle() const { return nullptr; }
    };

    class ISystemEvent : public EventTraits {
    public:
        ISystemEvent() = default;
        virtual ~ISystemEvent() = default;

        virtual void OnMainWindowCreated(NativeWindow *window) = 0;
    };
    using SystemEvent = Event<ISystemEvent>;

} // namespace sky
