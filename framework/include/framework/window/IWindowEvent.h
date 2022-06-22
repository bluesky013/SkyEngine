//
// Created by Zach Lee on 2022/1/3.
//


#pragma once

#include <framework/event/Event.h>

namespace sky {

    class IWindowEvent : public EventTrait {
    public:
        using KeyType = void*;

        IWindowEvent() = default;
        virtual ~IWindowEvent() = default;

        virtual void OnWindowResize(uint32_t width, uint32_t height) {}
    };

}