//
// Created on 2026/10/17.
//
#pragma once
#include <aurora/adaptor/renderer/UiRenderFeature.h>
#include <map>
#include <optional>
#include <ui/IUITextureRegistry.h>
#include <variant>
#include <vector>

namespace sky::editor {
    class UiSnapshotProducer final : public ui::IUITextureRegistry {
    public:
        explicit UiSnapshotProducer(uint64_t deviceEpoch);
        ui::UITextureId RegisterTexture(const ui::UIImageData &) override;
        ui::UITextureId RegisterTextureAs(ui::UITextureId, const ui::UIImageData &) override;
        void            UpdateTexture(ui::UITextureId, const ui::UIImageData &) override;
        void            ReleaseTexture(ui::UITextureId) override;
        std::variant<aurora::RendererError, aurora::FrameExtensionPayload>
                 Snapshot(Name, aurora::ViewportId, aurora::Extent2D, const ui::UIDrawData &, aurora::ViewOutputRef) const;
        void     Flush(aurora::FramePacket &);
        uint64_t CpuBytes() const
        {
            return cpuBytes;
        }

    private:
        struct Texture {
            aurora::RenderResourceKey key;
            aurora::RenderResourceKey published;
            ui::UIImageData           image;
            bool                      dirty = true;
        };
        std::optional<aurora::RendererError>   Validate(const ui::UIImageData &, uint64_t previous = 0) const;
        uint64_t                               epoch = 0, cpuBytes = 0;
        ui::UITextureId                        nextId = 2;
        std::optional<aurora::RendererError>   failure;
        std::map<ui::UITextureId, Texture>     textures;
        std::vector<aurora::RenderResourceKey> released;
    };
} // namespace sky::editor
