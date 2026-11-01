//
// Created on 2026/10/30.
//

#pragma once

#include <aurora/adaptor/renderer/RenderSceneBridge.h>
#include <core/environment/Singleton.h>
#include <memory>
#include <string>
#include <vector>

namespace sky::editor {

    struct SceneOverlayContext {
        World                                        *world = nullptr;
        aurora::RendererInfo                          info;
        aurora::RenderEntityRef                       entity;
        std::shared_ptr<aurora::ISceneResourceSource> resources;
    };

    class ISceneOverlay {
    public:
        virtual ~ISceneOverlay()                                                             = default;
        virtual void Append(aurora::FramePacket &packet, const SceneOverlayContext &context) = 0;
        virtual void Retire(aurora::FramePacket &packet)                                     = 0;
        virtual void OnEvent(const aurora::RendererEvent &event)                             = 0;
        virtual void OnRendererStopped()                                                     = 0;
    };

    // Host-thread callbacks run once while recording a new packet; BUSY retries reuse that packet.
    class SceneOverlayRegistry : public Singleton<SceneOverlayRegistry> {
        friend class Singleton<SceneOverlayRegistry>;

    public:
        bool Add(std::string id, std::shared_ptr<ISceneOverlay> overlay, std::shared_ptr<const void> lease);
        void Remove(const std::string &id);
        void Append(aurora::FramePacket                                 &packet,
                    aurora::SceneId                                      scene,
                    World                                               *world,
                    aurora::RendererInfo                                 info,
                    const std::shared_ptr<aurora::ISceneResourceSource> &resources);
        void OnEvent(const aurora::RendererEvent &event);
        void OnRendererStopped();

    private:
        struct Entry {
            std::shared_ptr<const void>    lease;
            std::shared_ptr<ISceneOverlay> overlay;
            std::string                    id;
            uint32_t                       index    = 0;
            bool                           retiring = false;
        };
        std::vector<Entry> entries;
        // RenderSceneBridge owns [0, 8192); overlay indices never overlap or recycle within a renderer.
        uint32_t nextIndex = 8192;
    };

} // namespace sky::editor
