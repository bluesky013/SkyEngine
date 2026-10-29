//
// Created on 2026/10/17.
//

#pragma once

#include "frame/CpuRenderCoordinator.h"
#include <array>
#include <aurora/renderer/IRenderer.h>

namespace sky::aurora {
    RendererCreateResult CreateRuntimeRenderer(const RendererInitInfo &);

    namespace detail {
        template <typename Coordinator, typename EventSink>
        void DrainRuntimeCompletions(Coordinator &coordinator, EventSink &events)
        {
            std::array<CpuCoordinatorEvent, 1> batch;
            while (coordinator.PollEvents(batch)) {
                if (auto *packet = std::get_if<CpuPacketCompleted>(&batch[0])) {
                    if (packet->application.error)
                        events.Push(RendererFailure{packet->application.sequence, *packet->application.error});
                    else if (packet->executionError)
                        events.Push(RendererFailure{packet->application.sequence, *packet->executionError});
                    else if (!packet->preparation.Succeeded())
                        events.Push(RendererFailure{packet->application.sequence, {RendererErrorCode::INTERNAL_ERROR, packet->preparation.error}});
                    else
                        continue;
                } else {
                    const auto &stopped = std::get<CpuCoordinatorStopped>(batch[0]);
                    if (stopped.error) {
                        events.failed = true;
                        events.Push(RendererFailure{0, *stopped.error});
                    }
                    events.Push(RendererStopped{});
                }
                return;
            }
        }
    } // namespace detail
} // namespace sky::aurora
