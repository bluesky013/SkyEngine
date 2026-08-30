//
// Created by Zach Lee on 2026/8/8.
//

#pragma once

namespace sky::aurora {

    class RenderViewport {
    public:
        RenderViewport() = default;
        virtual ~RenderViewport() = default;

        virtual bool Acquire() = 0;
        virtual void Release() = 0;
    };

} // namespace sky::aurora
