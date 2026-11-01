//
// Created on 2026/10/30.
//

#pragma once

#include <editor/render/SceneOverlayRegistry.h>
#include <navigation/NaviMeshGenerator.h>
#include <recast/render/NavigationRenderBridge.h>

namespace sky::ai {

    class NavigationEditorService final : public editor::ISceneOverlay, public std::enable_shared_from_this<NavigationEditorService> {
    public:
        bool Register();
        void Unregister();
        void Deactivate();
        bool IsActive() const
        {
            return active;
        }
        void Tick();
        bool CanBuild() const;
        void RequestBuild();
        bool IsDebugVisible() const
        {
            return debugVisible;
        }
        const std::string &GetError() const
        {
            return error;
        }

        void Append(aurora::FramePacket &packet, const editor::SceneOverlayContext &context) override;
        void Retire(aurora::FramePacket &packet) override;
        void OnEvent(const aurora::RendererEvent &event) override;
        void OnRendererStopped() override;

    private:
        CounterPtr<NaviMeshGenerator> generator;
        WorldPtr                      requestedWorld, buildWorld;
        NavigationRenderBridge        render;
        bool                          active = true, registered = false, debugVisible = false;
        std::string                   error;
    };

} // namespace sky::ai
