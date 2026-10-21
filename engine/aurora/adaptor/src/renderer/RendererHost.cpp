//
// Created on 2026/10/17.
//

#include <aurora/adaptor/renderer/RendererHost.h>

#include <array>
#include <aurora/pipeline/SceneRenderFeature.h>
#include <chrono>
#include <core/logger/Logger.h>
#include <framework/interface/Interface.h>
#include <framework/platform/PlatformBase.h>
#include <thread>

namespace sky::aurora {
    std::optional<RendererError> RegisterScenePass(FramePacket &packet, RenderFeatureId id, std::shared_ptr<const void> moduleLease)
    {
        if (!id.IsValid() || !moduleLease)
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Scene pass registration requires a feature ID and defining module lease"};
        const Name name("aurora.scene");
        packet.featureCommands.push_back(RegisterRenderFeature{id, name, std::move(moduleLease), CreateSceneRenderFeature(name)});
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
        renderer->RequestShutdown();
        while (renderer->GetState() != RendererState::STOPPED) {
            std::array<RendererEvent, 32> events;
            const auto                    count = renderer->PollEvents(events);
            for (const auto &event : std::span(events).first(count))
                if (const auto *failure = std::get_if<RendererFailure>(&event))
                    LOG_E("RendererHost", "shutdown frame %llu: %s", failure->packetSequence, failure->error.message.c_str());
            if (Platform::Get()->GetImpl()) {
                bool exit = false;
                Platform::Get()->PoolEvent(exit);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto *service = Interface<IRenderer>::Get();
        if (service->GetApi() == renderer.get())
            service->UnRegister();
        renderer.reset();
    }
} // namespace sky::aurora
