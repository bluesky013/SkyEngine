#pragma once

#include <aurora/renderer/IRenderer.h>
#include <memory>
#include <optional>
#include <utility>

namespace sky::editor {
    class EditorViewportState {
    public:
        explicit EditorViewportState(uint32_t index) : viewport{index, 1}
        {
        }

        aurora::ViewportId Output() const
        {
            return viewport;
        }
        bool IsCreated() const
        {
            return created;
        }
        bool IsRetiring() const
        {
            return retiring;
        }

        bool OnFailure(const aurora::ViewportFailed &failure)
        {
            if (failure.viewport != viewport || !created || retiring)
                return false;
            replacement |= failure.requiresReplacement;
            return true;
        }

        std::optional<aurora::RendererError> OnDestroyed(const aurora::ViewportDestroyed &destroyed, bool closed)
        {
            if (destroyed.viewport != viewport || !retiring)
                return {};
            nativeLease.reset();
            created = retiring = replacement = false;
            if (!closed) {
                if (viewport.generation == UINT32_MAX)
                    return aurora::RendererError{aurora::RendererErrorCode::OUT_OF_MEMORY, "Editor viewport generation exhausted"};
                ++viewport.generation;
            }
            return {};
        }

        bool Prepare(aurora::FramePacket &packet, const aurora::ViewportDesc &descriptor, bool closed, std::shared_ptr<const void> lease)
        {
            if (retiring)
                return false;
            if (created && (closed || replacement)) {
                packet.commands.push_back(aurora::DestroyViewport{viewport});
                retiring = true;
                return false;
            }
            if (closed)
                return false;
            if (!created) {
                // The window owner and this native borrow survive every pending packet and retirement.
                nativeLease = std::move(lease);
                packet.commands.push_back(aurora::CreateViewport{viewport, descriptor});
                created = true;
            }
            return true;
        }

    private:
        aurora::ViewportId          viewport;
        std::shared_ptr<const void> nativeLease;
        bool                        created = false, retiring = false, replacement = false;
    };
} // namespace sky::editor
