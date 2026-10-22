//
// Created on 2026/10/22.
//

#pragma once

#include <algorithm>
#include <array>
#include <aurora/renderer/IRenderer.h>

namespace sky::editor {
    class EditorFramePacer {
    public:
        bool CanBuild() const
        {
            return std::find(sequences.begin(), sequences.end(), 0) != sequences.end();
        }
        aurora::PacketSubmitResult TrySubmit(aurora::IRenderer &renderer, std::unique_ptr<aurora::FramePacket> &packet)
        {
            const auto slot = std::find(sequences.begin(), sequences.end(), 0);
            if (slot == sequences.end())
                return {aurora::PacketSubmitStatus::BUSY};
            const auto sequence = packet ? packet->sequence : 0;
            const auto result   = renderer.TrySubmit(packet);
            if (result.status == aurora::PacketSubmitStatus::ACCEPTED)
                *slot = sequence;
            return result;
        }
        void OnEvent(const aurora::RendererEvent &event)
        {
            // CPU application is not retirement; even skipped frames have a FrameCompleted receipt.
            if (const auto *completed = std::get_if<aurora::FrameCompleted>(&event)) {
                for (auto &sequence : sequences)
                    if (sequence == completed->sequence)
                        sequence = 0;
            }
        }
        void Reset()
        {
            sequences.fill(0);
        }

    private:
        std::array<uint64_t, 2> sequences{};
    };
} // namespace sky::editor
