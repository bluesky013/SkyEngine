//
// Created by blues on 2025/2/3.
//

#pragma once

#include <core/event/Event.h>

namespace sky {
    class Actor;
    struct Transform;

    struct ITransformEvent : public EventTraits {
        using KeyType   = Actor*;
        using MutexType = void;

        virtual void OnTransformChanged(const Transform& global, const Transform& local) = 0;
    };
    using TransformEvent = Event<ITransformEvent>;

} // namespace sky
