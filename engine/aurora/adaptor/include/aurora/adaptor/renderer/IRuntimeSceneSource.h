//
// Created on 2026/10/28.
//

#pragma once

#include <aurora/renderer/RenderCommands.h>
#include <optional>

namespace sky::aurora {

    struct RuntimeSceneContext {
        SceneId      scene;
        ViewId       view;
        ViewportId   output;
        Extent2D     extent;
        RendererInfo renderer;
    };

    // Register through Interface<IRuntimeSceneSource> while the owning module is started.
    // Main-thread calls only; packets must own their data and must not retain the source.
    class IRuntimeSceneSource {
    public:
        virtual ~IRuntimeSceneSource()                                                                       = default;
        virtual std::optional<RendererError> Append(FramePacket &packet, const RuntimeSceneContext &context) = 0;
        virtual void                         OnRendererEvent(const RendererEvent &event)
        {
        }
    };

} // namespace sky::aurora
