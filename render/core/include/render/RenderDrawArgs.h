//
// Created by Zach Lee on 2023/8/19.
//

#pragma once

#include <variant>
#include <functional>
#include <rhi/CommandBuffer.h>
#include <core/std/Container.h>

namespace sky {

    using DrawArgs = std::variant<rhi::CmdDrawLinear, rhi::CmdDrawIndexed, rhi::Viewport, rhi::Rect2D>;

    struct RenderCmdList {
        std::function<void(rhi::GraphicsEncoder&)> fn;
    };

} // namespace sky
