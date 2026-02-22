//
// Created by blues on 2024/7/28.
//

#pragma once

#include <core/event/Event.h>

namespace sky {

    class ITickEvent : public EventTraits {
    public:
        ITickEvent() = default;
        virtual ~ITickEvent() = default;

        using KeyType   = void;
        using MutexType = void;

        virtual void Tick(float time) = 0;
    };
    using TickEvent = Event<ITickEvent>;

} // namespace sky
