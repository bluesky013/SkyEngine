//
// Created by Zach Lee on 2023/8/19.
//

#pragma once

#include <variant>
#include <functional>
#include <rhi/CommandBuffer.h>

namespace sky {

    struct RenderPackage {
        std::function<void(rhi::GraphicsEncoder&)> fn;
    };

    using DrawArgs = std::variant<rhi::CmdDrawLinear, rhi::CmdDrawIndexed>;

} // namespace sky
