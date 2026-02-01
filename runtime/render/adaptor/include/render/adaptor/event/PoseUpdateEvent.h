//
// Created by Zach Lee on 2026/2/1.
//

#pragma once

#include <core/event/Event.h>

namespace sky {

    class Actor;
    struct AnimFinalPose;

    struct IPoseUpdateEvent : public EventTraits {
        using KeyType   = Actor*;
        using MutexType = void;

        virtual void OnPoseUpdated(const AnimFinalPose& pose) = 0;
    };
    using PoseUpdateEvent = Event<IPoseUpdateEvent>;

} // namespace sky