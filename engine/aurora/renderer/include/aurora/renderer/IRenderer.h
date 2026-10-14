#pragma once

#include <aurora/renderer/RenderCommands.h>
#include <memory>
#include <span>

namespace sky::aurora {

    // TrySubmit, PollEvents and RequestShutdown share one host producer thread.
    class IRenderer {
    public:
        virtual ~IRenderer() = default;

        // ACCEPTED transfers ownership and clears packet;
        // BUSY/REJECTED preserve it unchanged. Acceptance is not frame completion.
        virtual PacketSubmitResult TrySubmit(std::unique_ptr<FramePacket> &packet) = 0;
        // Drain events once up to capacity; zero capacity consumes nothing.
        virtual size_t        PollEvents(std::span<RendererEvent> destination) = 0;
        virtual RendererState GetState() const                                 = 0;
        // Device loss is terminal; replacement renderers receive a new epoch.
        virtual RendererInfo GetInfo() const = 0;

        // Nonblocking/idempotent, including FAILED; accepted packets must drain or report failure.
        // Pump platform events until RendererStopped; destroy renderer/unload provider only after STOPPED.
        virtual void RequestShutdown() = 0;
    };

    using RendererCreateResult = std::variant<RendererError, std::unique_ptr<IRenderer>>;

    // Hosts resolve this service through sky::Interface<IRendererProvider>.
    class IRendererProvider {
    public:
        virtual ~IRendererProvider() = default;
        // Success is non-null and initialized; keep the provider loaded until all renderers stop and are destroyed.
        virtual RendererCreateResult Create(const RendererInitInfo &info) = 0;
    };

} // namespace sky::aurora
