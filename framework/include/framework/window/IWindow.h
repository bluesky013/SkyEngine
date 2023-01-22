//
// Created by Zach Lee on 2023/1/22.
//

#pragma once

#include <memory>

namespace sky {

    class IWindow {
    public:
        IWindow() = default;
        virtual ~IWindow() = default;

        virtual void *GetNativeHandle() const = 0;
    };


} // namespace sky
