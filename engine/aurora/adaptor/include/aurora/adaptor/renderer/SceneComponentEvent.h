#pragma once

#include <core/event/Event.h>
#include <framework/world/Actor.h>

namespace sky::aurora {
    struct ISceneComponentEvent : EventTraits {
        using KeyType                                                                 = World *;
        virtual void OnSceneComponentChanged(ComponentBase &component, bool attached) = 0;
    };

    template <typename Data>
    class SceneComponentAdaptor : public ComponentAdaptor<Data> {
    public:
        void OnAttachToWorld() override
        {
            Notify(true);
        }
        void OnDetachFromWorld() override
        {
            Notify(false);
        }

    private:
        void Notify(bool attached)
        {
            if (this->actor && this->actor->GetWorld())
                Event<ISceneComponentEvent>::BroadCast(this->actor->GetWorld(), &ISceneComponentEvent::OnSceneComponentChanged, *this, attached);
        }
    };
} // namespace sky::aurora
