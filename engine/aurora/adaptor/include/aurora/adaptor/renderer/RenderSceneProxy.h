//
// Created on 2026/10/17.
//

#pragma once

#include <aurora/renderer/RenderCommands.h>
#include <optional>

namespace sky::aurora {
    // Main-thread lifecycle only; commands belong to the host's owned FIFO packet.
    class RenderSceneProxy {
    public:
        RenderSceneProxy()                                    = default;
        RenderSceneProxy(const RenderSceneProxy &)            = delete;
        RenderSceneProxy &operator=(const RenderSceneProxy &) = delete;

        std::optional<RendererError> Create(FramePacket &, SceneId, SceneDesc = {});
        std::optional<RendererError> Destroy(FramePacket &);
        void                         OnEvent(const RendererEvent &);
        void                         OnRendererStopped();
        SceneId                      GetSceneId() const;
        bool                         IsRetiring() const;

    private:
        SceneId id;
        bool    retiring = false;
    };
} // namespace sky::aurora
