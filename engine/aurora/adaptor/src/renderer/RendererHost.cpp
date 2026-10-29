//
// Created on 2026/10/17.
//

#include <aurora/adaptor/renderer/RendererHost.h>

#include "RendererShutdown.h"
#include <aurora/pipeline/SceneRenderFeature.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>

namespace sky::aurora {
    std::optional<RendererError> RegisterScenePass(FramePacket                      &packet,
                                                   RenderFeatureId                   id,
                                                   std::shared_ptr<const void>       moduleLease,
                                                   std::shared_ptr<ShaderFileSystem> sources,
                                                   FileSystemPtr                     products)
    {
        if (!id.IsValid() || !moduleLease)
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Scene pass registration requires a feature ID and defining module lease"};
        const Name name("aurora.scene");
        packet.featureCommands.push_back(
            RegisterRenderFeature{id, name, std::move(moduleLease), CreateSceneRenderFeature(name, std::move(sources), std::move(products))});
        return {};
    }
    RendererHost::RendererHost() = default;
    RendererHost::~RendererHost()
    {
        Shutdown();
    }
    std::optional<RendererError> RendererHost::Init(IRendererProvider &provider, const RendererInitInfo &info)
    {
        auto *service = Interface<IRenderer>::Get();
        if (renderer || service->GetApi())
            return RendererError{RendererErrorCode::NOT_READY, "A renderer host is already active"};
        auto created = provider.Create(info);
        if (const auto *error = std::get_if<RendererError>(&created))
            return *error;
        renderer = std::move(std::get<std::unique_ptr<IRenderer>>(created));
        if (!renderer)
            return RendererError{RendererErrorCode::INTERNAL_ERROR, "Provider returned an empty renderer"};
        service->Register(*renderer);
        return {};
    }
    IRenderer *RendererHost::Get() const
    {
        return renderer.get();
    }
    void RendererHost::Shutdown()
    {
        if (!renderer)
            return;
        detail::DrainRenderer(*renderer, [](EventPollMode mode) {
            if (Platform::Get()->GetImpl()) {
                bool exit = false;
                Platform::Get()->PoolEvent(exit, mode);
            }
        });
        auto *service = Interface<IRenderer>::Get();
        if (service->GetApi() == renderer.get())
            service->UnRegister();
        renderer.reset();
    }
} // namespace sky::aurora
