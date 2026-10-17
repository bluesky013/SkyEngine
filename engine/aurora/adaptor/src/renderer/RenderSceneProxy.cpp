//
// Created on 2026/10/17.
//

#include <aurora/adaptor/renderer/RenderSceneProxy.h>

namespace sky::aurora {
    std::optional<RendererError> RenderSceneProxy::Create(FramePacket &packet, SceneId next, SceneDesc desc)
    {
        if (id.IsValid() || !next.IsValid())
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Scene proxy requires a free, valid scene handle"};
        packet.commands.push_back(CreateScene{next, std::move(desc)});
        id = next;
        return {};
    }
    std::optional<RendererError> RenderSceneProxy::Destroy(FramePacket &packet)
    {
        if (!id.IsValid() || retiring)
            return RendererError{RendererErrorCode::STALE_HANDLE, "Scene proxy is absent or retiring"};
        packet.commands.push_back(DestroyScene{id});
        retiring = true;
        return {};
    }
    void RenderSceneProxy::OnEvent(const RendererEvent &event)
    {
        if (const auto *destroyed = std::get_if<SceneDestroyed>(&event); destroyed && destroyed->scene == id && retiring) {
            id       = {};
            retiring = false;
        }
    }
    void RenderSceneProxy::OnRendererStopped()
    {
        id       = {};
        retiring = false;
    }
    SceneId RenderSceneProxy::GetSceneId() const
    {
        return retiring ? SceneId{} : id;
    }
    bool RenderSceneProxy::IsRetiring() const
    {
        return retiring;
    }
} // namespace sky::aurora
