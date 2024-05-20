//
// Created by blues on 2024/5/21.
//

#pragma once

#include <core/event/Event.h>
#include <core/math/Matrix4.h>

namespace sky {
    class Actor;

    class IMainViewportEvent {
    public:
        IMainViewportEvent() = default;
        virtual ~IMainViewportEvent() = default;

        using KeyType   = void;
        using MutexType = void;

        virtual void Active(Actor* actor) = 0;
        virtual void DeActive() = 0;
    };
    using MainViewportEvent = Event<IMainViewportEvent>;

} // namespace sky
