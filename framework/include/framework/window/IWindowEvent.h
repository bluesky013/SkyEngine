//
// Created by Zach Lee on 2022/1/3.
//


#pragma once

namespace sky {

    class IWindowEvent {
    public:
        IWindowEvent() = default;
        virtual ~IWindowEvent() = default;

        virtual void OnWindowResize(void* window, uint32_t width, uint32_t height) {}
    };

}