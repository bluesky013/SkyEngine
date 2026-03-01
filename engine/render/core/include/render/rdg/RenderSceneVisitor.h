//
// Created by Zach Lee on 2023/8/27.
//

#pragma once
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <core/std/Container.h>
#include <core/platform/Platform.h>

namespace sky {
    class RenderScene;
    class SceneView;
    struct RenderPrimitive;
} // namespace sky

namespace sky::rdg {
    struct RenderGraph;

    constexpr uint64_t PRIMITIVE_ACTIVE_BIT = 1LLU << 63;
    constexpr uint64_t PRIMITIVE_VIEW_MASK = ~PRIMITIVE_ACTIVE_BIT;

    struct PrimitiveVisibleInfo {
        FORCEINLINE void SetActive()
        {
            visibleMask |= PRIMITIVE_ACTIVE_BIT;
        }

        FORCEINLINE bool IsActive() const
        {
            return (visibleMask & PRIMITIVE_ACTIVE_BIT) == PRIMITIVE_ACTIVE_BIT;
        }

        FORCEINLINE void SetActiveInView(uint8_t viewId)
        {
            visibleMask |= (1LLU << viewId);
        }

        FORCEINLINE bool IsActiveInView(uint8_t viewId) const
        {
            const uint32_t viewBit = 1LLU << viewId;
            return (visibleMask & viewBit) == viewBit;
        }

        // 0-62 bits for view mask, 63 bit for active flag.
        // 63 bit is set when the primitive is active in current frame, otherwise it's invisible and can be skipped in render queue dispatch.
        uint64_t visibleMask = 0;
    };

    struct RenderSceneVisitor{
        explicit RenderSceneVisitor(RenderGraph &g, RenderScene *scn);

        [[maybe_unused]] void Execute();

        void PerformCulling();
        void DispatchToRenderQueue();

        RenderGraph &graph;
        RenderScene *scene;
        const PmrVector<RenderPrimitive *> &primitives;
        PmrVector<PrimitiveVisibleInfo> visibleInfos;
    };

} // namespace sky::rdg
