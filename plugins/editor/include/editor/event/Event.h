//
// Created by blues on 2024/3/29.
//

#pragma once

#include <core/event/Event.h>
#include <editor/event/EventId.h>

namespace sky::editor {

    class IToggleEvent {
    public:
        IToggleEvent() = default;
        virtual ~IToggleEvent() = default;

        using KeyType   = EventID;
        using MutexType = void;

        virtual void OnToggle(bool val) = 0;
    };
    using ToogleEvent = Event<IToggleEvent>;

    class IButtonEvent {
    public:
        IButtonEvent() = default;
        virtual ~IButtonEvent() = default;

        using KeyType   = EventID;
        using MutexType = void;

        virtual void OnClicked() = 0;
    };
    using ButtonEvent = Event<IButtonEvent>;

    template <typename T>
    class EventBinder {
    public:
        EventBinder() = default;
        ~EventBinder()
        {
            if (val != nullptr) {
                Event<T>::DisConnect(val);
            }
        }

        using KeyType = T::KeyType;

        void Bind(T *inter, KeyType key)
        {
            Event<T>::Connect(key, inter);
            val = inter;
        }

    private:
        T *val = nullptr;
    };

} // sky::editor