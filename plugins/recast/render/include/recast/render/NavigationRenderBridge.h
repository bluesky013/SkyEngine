//
// Created on 2026/10/30.
//

#pragma once

#include <aurora/adaptor/renderer/RenderSceneBridge.h>
#include <navigation/NaviDebugGeometry.h>

namespace sky::ai {

    // The host supplies a collision-free entity slot and owns packet submission/retirement.
    class NavigationRenderBridge {
    public:
        bool               Append(aurora::FramePacket          &packet,
                                  aurora::RenderEntityRef       entity,
                                  aurora::RendererInfo          info,
                                  const NaviDebugGeometry      &geometry,
                                  aurora::ISceneResourceSource &resources);
        void               Clear(aurora::FramePacket &packet);
        void               OnEvent(const aurora::RendererEvent &event);
        void               OnRendererStopped();
        const std::string &GetError() const
        {
            return error;
        }

    private:
        bool                      PrepareMaterial(aurora::FramePacket &packet, aurora::RendererInfo info, aurora::ISceneResourceSource &resources);
        aurora::RenderEntityRef   entity;
        aurora::RenderResourceKey mesh, material;
        uint64_t                  deviceEpoch = 0, geometryHash = 0, revision = 0;
        uint32_t                  generation = 1;
        bool                      created = false, retiring = false;
        std::string               error;
    };

} // namespace sky::ai
