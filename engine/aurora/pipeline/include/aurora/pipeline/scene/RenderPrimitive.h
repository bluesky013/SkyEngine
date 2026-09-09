//
// Aurora render primitive: geometry + per-tag technique bindings.
// Persistent object; internal containers must NOT bind the frame arena.
//

#pragma once

#include <core/name/Name.h>
#include <core/shapes/AABB.h>
#include <aurora/rdg/CompiledGraph.h>

#include <unordered_map>

namespace sky::aurora {

    class ResourceGroup;
    class GraphicsPipeline;
    class SceneView;

    // per-tag binding: what to draw when a queue with this tag collects
    struct TechniqueBinding {
        GraphicsPipeline *pso                = nullptr;
        ResourceGroup    *batchResourceGroup = nullptr; // set 2
    };

    // gather context passed by the collector (pass template)
    struct GatherContext {
        Name tag;                    // empty = no filter (collect default binding)
        const SceneView *view = nullptr;

        DrawItem item{};             // out: filled by GatherRenderItem when tag matches
        bool     gathered = false;   // out
    };

    struct RenderPrimitive {
        // geometry
        Buffer  *vb       = nullptr;
        Buffer  *ib       = nullptr;
        uint32_t vbOffset = 0;
        uint32_t ibOffset = 0;
        CmdDrawIndexed args{};

        AABB worldBounds{};

        // tag -> binding; empty-name key is the default binding
        std::unordered_map<Name, TechniqueBinding> techniques;

        void GatherRenderItem(GatherContext &ctx) const;
    };

} // namespace sky::aurora
