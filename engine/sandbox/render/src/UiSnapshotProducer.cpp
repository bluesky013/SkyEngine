//
// Created on 2026/10/17.
//
#include <editor/render/UiSnapshotProducer.h>

#include <algorithm>

namespace sky::editor {
    using namespace aurora;
    UiSnapshotProducer::UiSnapshotProducer(uint64_t deviceEpoch) : epoch(deviceEpoch)
    {
        ui::UIImageData white{1, 1, {255, 255, 255, 255}};
        textures.emplace(0, Texture{{Uuid::Create(), 1, 0, epoch}, {}, std::move(white)});
        cpuBytes = 4;
    }
    std::optional<RendererError> UiSnapshotProducer::Validate(const ui::UIImageData &image, uint64_t previous) const
    {
        if (!image.width || !image.height || image.width > 16384 || image.height > 16384 ||
            uint64_t(image.width) * image.height * 4 != image.pixels.size())
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Invalid complete UI RGBA8 image"};
        if (image.pixels.size() > 64ull * 1024 * 1024 - (cpuBytes - previous))
            return RendererError{RendererErrorCode::OUT_OF_MEMORY, "UI CPU image budget exceeded"};
        return {};
    }
    ui::UITextureId UiSnapshotProducer::RegisterTexture(const ui::UIImageData &image)
    {
        while (nextId != UINT32_MAX && (textures.contains(nextId) || views.contains(nextId)))
            ++nextId;
        if (nextId == UINT32_MAX) {
            failure = RendererError{RendererErrorCode::OUT_OF_MEMORY, "UI texture IDs exhausted"};
            return 0;
        }
        return RegisterTextureAs(nextId++, image);
    }
    ui::UITextureId UiSnapshotProducer::RegisterTextureAs(ui::UITextureId id, const ui::UIImageData &image)
    {
        if (id < 2 || views.contains(id))
            return 0;
        if (textures.contains(id))
            return id;
        if (textures.size() + released.size() >= 1024) {
            failure = RendererError{RendererErrorCode::OUT_OF_MEMORY, "UI texture count budget exceeded"};
            return 0;
        }
        if (auto error = Validate(image)) {
            failure = std::move(error);
            return 0;
        }
        textures.emplace(id, Texture{{Uuid::Create(), 1, 0, epoch}, {}, image});
        cpuBytes += image.pixels.size();
        return id;
    }
    void UiSnapshotProducer::UpdateTexture(ui::UITextureId id, const ui::UIImageData &image)
    {
        const auto found = textures.find(id);
        if (id < 2 || found == textures.end())
            return;
        if (auto error = Validate(image, found->second.image.pixels.size())) {
            failure = std::move(error);
            return;
        }
        auto &texture = found->second;
        if (texture.key.revision == UINT64_MAX) {
            failure = RendererError{RendererErrorCode::OUT_OF_MEMORY, "UI texture revisions exhausted"};
            return;
        }
        cpuBytes -= texture.image.pixels.size();
        texture.image = image;
        cpuBytes += texture.image.pixels.size();
        ++texture.key.revision;
        texture.dirty = true;
    }
    void UiSnapshotProducer::ReleaseTexture(ui::UITextureId id)
    {
        const auto found = textures.find(id);
        if (id < 2 || found == textures.end())
            return;
        if (found->second.published.product)
            released.push_back(found->second.published);
        cpuBytes -= found->second.image.pixels.size();
        textures.erase(found);
    }
    ui::UITextureId UiSnapshotProducer::RegisterView(ViewOutputRef view)
    {
        while (nextId != UINT32_MAX && (textures.contains(nextId) || views.contains(nextId)))
            ++nextId;
        if (nextId == UINT32_MAX || views.size() >= 64)
            return 0;
        const auto id = nextId++;
        views.emplace(id, view);
        return id;
    }
    bool UiSnapshotProducer::UpdateView(ui::UITextureId id, ViewOutputRef view)
    {
        const auto found = views.find(id);
        if (found == views.end())
            return false;
        found->second = view;
        return true;
    }
    void UiSnapshotProducer::ReleaseView(ui::UITextureId id)
    {
        views.erase(id);
    }
    void UiSnapshotProducer::Flush(FramePacket &packet)
    {
        for (auto &[id, texture] : textures) {
            if (!texture.dirty)
                continue;
            if (texture.published.product)
                released.push_back(texture.published);
            RenderResourcePayload payload;
            payload.resource       = texture.key;
            payload.format         = Name("ui.rgba8");
            payload.bytes          = texture.image.pixels;
            payload.metadata.image = RenderImageLayout{texture.image.width, texture.image.height};
            packet.commands.push_back(ResourceCpuReady{std::move(payload)});
            texture.published = texture.key;
            texture.dirty     = false;
        }
        packet.releasedResources.insert(packet.releasedResources.end(), released.begin(), released.end());
        released.clear();
    }
    std::variant<RendererError, FrameExtensionPayload>
    UiSnapshotProducer::Snapshot(Name feature, ViewportId output, Extent2D extent, const ui::UIDrawData &data, ViewOutputRef content) const
    {
        if (failure)
            return *failure;
        if (!extent.width || !extent.height)
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "UI snapshot requires a nonempty extent"};
        RenderUiSnapshot snapshot;
        snapshot.output  = output;
        snapshot.indices = data.indices;
        snapshot.vertices.reserve(data.vertices.size());
        for (const auto &source : data.vertices) {
            RenderUiVertex vertex;
            vertex.x = source.x / extent.width;
            vertex.y = source.y / extent.height;
            vertex.u = source.u;
            vertex.v = source.v;
            for (uint32_t channel = 0; channel < 4; ++channel)
                vertex.color[channel] = float((source.color >> (channel * 8)) & 255) / 255;
            vertex.shape  = {source.u - source.roundCenterX, source.v - source.roundCenterY, source.roundHalfX, source.roundHalfY};
            vertex.radius = source.roundRadius;
            snapshot.vertices.push_back(vertex);
        }
        for (const auto &source : data.commands) {
            if (!source.indexCount)
                continue;
            RenderUiDraw draw;
            draw.firstIndex = source.indexOffset;
            draw.indexCount = source.indexCount;
            draw.clip       = {std::clamp(source.clip.left / extent.width, 0.f, 1.f), std::clamp(source.clip.top / extent.height, 0.f, 1.f),
                               std::clamp(source.clip.right / extent.width, 0.f, 1.f), std::clamp(source.clip.bottom / extent.height, 0.f, 1.f)};
            if (source.textureId == 1 && !source.shape)
                draw.texture = content;
            else if (const auto viewBinding = views.find(source.textureId); !source.shape && viewBinding != views.end())
                draw.texture = viewBinding->second;
            else {
                const auto found = textures.find(source.shape ? 0 : source.textureId);
                if (found == textures.end())
                    continue;
                draw.image = found->second.key;
            }
            snapshot.draws.push_back(draw);
        }
        return EncodeRenderUiSnapshot(feature, snapshot);
    }
} // namespace sky::editor
