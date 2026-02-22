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
    };

    class ISystemEvent : public EventTraits {
    public:
        ISystemEvent() = default;
        virtual ~ISystemEvent() = default;

        virtual void OnMainWindowCreated(NativeWindow *window) = 0;
    };
    using SystemEvent = Event<ISystemEvent>;

} // namespace sky
