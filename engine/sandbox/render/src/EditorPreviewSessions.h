#pragma once

#include <aurora/adaptor/renderer/RenderSceneBridge.h>
#include <aurora/adaptor/renderer/RenderSceneProxy.h>
#include <deque>
#include <editor/core/asset/AssetPreviewProvider.h>
#include <editor/render/UiSnapshotProducer.h>

namespace sky::editor {
    class EditorPreviewSessions final : public IAssetPreviewProvider {
    public:
        EditorPreviewSessions(aurora::RendererInfo                          info,
                              std::shared_ptr<aurora::ISceneResourceSource> resources,
                              UiSnapshotProducer                           &textures,
                              std::function<aurora::ViewportId()>           allocate);
        AssetPreviewResult                   CreateSession(AssetPreviewScene scene) override;
        AssetPreviewStatusResult             Update(AssetPreviewHandle handle, EditorCamera camera) override;
        AssetPreviewStatusResult             Resize(AssetPreviewHandle handle, uint32_t width, uint32_t height) override;
        AssetPreviewStatusResult             CloseSession(AssetPreviewHandle handle) override;
        AssetPreviewStatusResult             Status(AssetPreviewHandle handle) const override;
        std::optional<aurora::RendererError> Append(aurora::FramePacket &packet);
        void                                 OnEvent(const aurora::RendererEvent &event);
        void                                 OnRendererStopped();
        bool                                 SetResources(std::shared_ptr<aurora::ISceneResourceSource> source);
        size_t                               Count() const
        {
            return sessions.size();
        }

    private:
        struct Session {
            AssetPreviewScene                          input;
            AssetPreviewHandle                         handle;
            aurora::RenderSceneProxy                   scene;
            std::unique_ptr<aurora::RenderSceneBridge> bridge;
            aurora::ViewportId                         output;
            aurora::ViewId                             view;
            uint32_t                                   texture = 0;
            aurora::Extent2D                           submitted;
            uint64_t                                   outputRevision = 0, viewRevision = 0, createdSequence = 0;
            bool                                       created = false, closing = false, retiring = false;
            bool                                       sceneRetired = false, viewRetired = false, outputRetired = false;
            std::string                                error;
        };
        Session                                      *Find(AssetPreviewHandle handle);
        void                                          RetireClosed();
        Uuid                                          generation = Uuid::Create();
        aurora::RendererInfo                          info;
        std::shared_ptr<aurora::ISceneResourceSource> resources;
        UiSnapshotProducer                           &textures;
        std::function<aurora::ViewportId()>           allocate;
        std::vector<std::unique_ptr<Session>>         sessions;
        std::deque<AssetPreviewHandle>                retired;
        uint32_t                                      nextScene         = 1;
        uint64_t                                      completedSequence = 0;
        bool                                          stopped           = false;
    };
} // namespace sky::editor
