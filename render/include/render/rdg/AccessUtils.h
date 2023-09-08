//
// Created by Zach Lee on 2023/7/23.
//

#pragma once

#include <rhi/Core.h>
#include <render/rdg/RenderGraphTypes.h>

namespace sky::rdg {
    struct RenderGraph;

    rhi::AccessFlags GetAccessFlags(const DependencyInfo &deps);
    rhi::AccessFlags GetImportAccessFlags(const RenderGraph &graph, VertexType resID);

    AccessRange GetAccessRange(const RenderGraph &graph, VertexType resID);
    void MergeSubRange(AccessRange &result, const AccessRange &val);

} // namespace sky::rdg
