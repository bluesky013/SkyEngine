//
// Created by Zach Lee on 2023/7/23.
//

#pragma once

#include <rhi/Core.h>
#include <render/rdg/RenderGraphTypes.h>

namespace sky::rdg {

    rhi::AccessFlags GetAccessFlags(const DependencyInfo &deps);
    rhi::ImageLayout GetImageLayout(const DependencyInfo &edge);
    void MergeSubRange(AccessRange &result, const AccessRange &val);
    bool Intersection(const AccessRange &lhs, const AccessRange &rhs);

} // namespace sky::rdg