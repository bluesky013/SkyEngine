//
// Created on 2026/10/29.
//

#pragma once

#include <array>
#include <aurora/renderer/IRenderer.h>
#include <chrono>
#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>
#include <thread>

namespace sky::aurora::detail {
    template <typename EventPump>
    void DrainRenderer(IRenderer &renderer, EventPump &&pump)
    {
        const auto poll = [&renderer] {
            std::array<RendererEvent, 32> events;
            const auto                    count = renderer.PollEvents(events);
            for (const auto &event : std::span(events).first(count))
                if (const auto *failure = std::get_if<RendererFailure>(&event))
                    LOG_E("RendererHost", "shutdown frame %llu: %s", static_cast<unsigned long long>(failure->packetSequence),
                          failure->error.message.c_str());
            return count;
        };
        renderer.RequestShutdown();
        while (renderer.GetState() != RendererState::STOPPED) {
            poll();
            if (renderer.GetState() == RendererState::STOPPED)
                break;
            // GPU/CPU completion must remain observable without another OS event.
            pump(EventPollMode::NON_BLOCKING);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        while (poll() != 0) {
        }
    }
} // namespace sky::aurora::detail
