//
// Created by Zach Lee on 2023/7/23.
//

#pragma once

#include <rhi/Core.h>
#include <render/rdg/RenderGraphTypes.h>

namespace sky::rdg {

    std::vector<rhi::AccessFlag> GetAccessFlag(const AccessEdge &edge);
    rhi::ImageLayout GetImageLayout(const AccessEdge &edge);

} // namespace sky::rdg