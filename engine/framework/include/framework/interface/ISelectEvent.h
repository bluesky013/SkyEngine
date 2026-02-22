//
// Created by blues on 2024/7/31.
//

#pragma once

#include <core/event/Event.h>

namespace sky {
    class Actor;

    class ISelectEvent : public EventTraits {
    public:
        ISelectEvent() = default;
        virtual ~ISelectEvent() = default;

        virtual void OnActorSelected(Actor *actor) = 0;
    };
    using SelectEvent = Event<ISelectEvent>;

} // namespace sky