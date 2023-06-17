//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <memory>

namespace sky::rdg {

    class RenderQueue {
    public:
        RenderQueue() = default;
        ~RenderQueue() = default;
    };
    using RenderQueuePtr = std::shared_ptr<RenderQueue>;

} // namespace sky