#include <aurora/adaptor/renderer/SceneAssetSource.h>

#include <algorithm>
#include <cstring>

namespace sky::aurora {
    std::variant<RendererError, RenderResourcePayload> ConvertSceneImage(const Asset<Texture> &asset, RendererInfo info, SceneImageUsage usage)
    {
        if (usage != SceneImageUsage::ENVIRONMENT && usage != SceneImageUsage::MATERIAL)
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Unknown scene image usage"};
        const auto &data = asset.Data();
        if (data.type != ImageAssetType::TEXTURE_2D || data.depth != 1 || data.arrayLayers != 1 ||
            (data.format != PixelFormat::RGBA8_UNORM && data.format != PixelFormat::RGBA16_SFLOAT &&
             (usage != SceneImageUsage::MATERIAL || data.format != PixelFormat::RGBA8_SRGB))) {
            return RendererError{.code = RendererErrorCode::UNSUPPORTED, .message = "Environment inputs require linear RGBA8/RGBA16_FLOAT 2D images"};
        }
        const RenderImageLayout layout{.width       = data.width,
                                       .height      = data.height,
                                       .mipLevels   = data.mipLevels,
                                       .arrayLayers = data.arrayLayers,
                                       .format      = data.format == PixelFormat::RGBA16_SFLOAT ? RenderImageFormat::RGBA16_FLOAT
                                                      : data.format == PixelFormat::RGBA8_SRGB  ? RenderImageFormat::RGBA8_SRGB
                                                                                                : RenderImageFormat::RGBA8_UNORM};
        const auto              bytes = RenderImageByteSize(layout);
        if (!asset.GetUuid() || (info.deviceEpoch == 0u) || data.version != ImageAssetData::CURRENT_VERSION || !bytes ||
            *bytes > 64ull * 1024 * 1024 || data.dataSize > 64ull * 1024 * 1024 || data.slices.size() != data.mipLevels ||
            (!data.rawData.empty() && data.rawData.size() != data.dataSize)) {
            return RendererError{.code = RendererErrorCode::INVALID_ARGUMENT, .message = "Environment image metadata or byte budget is invalid"};
        }
        RenderResourcePayload result;
        result.resource       = {.product      = asset.GetUuid(),
                                 .revision     = 1,
                                 .usageVariant = usage == SceneImageUsage::ENVIRONMENT ? 2u : 3u,
                                 .deviceEpoch  = info.deviceEpoch};
        result.format         = Name(usage == SceneImageUsage::ENVIRONMENT ? "linear-environment" : "material-image");
        result.metadata.image = layout;
        result.bytes.resize(*bytes);
        std::vector<std::pair<uint64_t, uint64_t>> ranges;
        uint64_t                                   destination = 0;
        for (uint32_t mip = 0; mip < data.mipLevels; ++mip) {
            const auto     found = std::ranges::find_if(data.slices, [&](const auto &slice) { return slice.mipLevel == mip; });
            const uint64_t size = uint64_t(std::max(1u, data.width >> mip)) * std::max(1u, data.height >> mip) * RenderImageTexelBytes(layout.format);
            if (found == data.slices.end() || (found->layer != 0u) || (found->depth != 0u) || found->size != size || found->offset > data.dataSize ||
                size > data.dataSize - found->offset ||
                std::ranges::any_of(ranges, [&](auto range) { return found->offset < range.second && range.first < found->offset + size; })) {
                return RendererError{.code    = RendererErrorCode::INVALID_ARGUMENT,
                                     .message = "Environment mip slices are incomplete, overlapping or out of bounds"};
            }
            ranges.emplace_back(found->offset, found->offset + size);
            if (data.rawData.empty()) {
                const auto bulk = asset.ReadBulk(found->offset, size);
                if (bulk.size() != size) {
                    return RendererError{.code = RendererErrorCode::INVALID_ARGUMENT, .message = "Environment mip bulk read is incomplete"};
                }
                std::memcpy(result.bytes.data() + destination, bulk.data(), size);
            } else {
                std::memcpy(result.bytes.data() + destination, data.rawData.data() + found->offset, size);
            }
            destination += size;
        }
        if (usage == SceneImageUsage::ENVIRONMENT && layout.format == RenderImageFormat::RGBA16_FLOAT) {
            for (size_t offset = 0; offset < result.bytes.size(); offset += sizeof(uint16_t)) {
                uint16_t value;
                std::memcpy(&value, result.bytes.data() + offset, sizeof(value));
                if ((value & 0x7c00) == 0x7c00 || (((value & 0x8000) != 0) && ((value & 0x7fff) != 0))) {
                    return RendererError{.code    = RendererErrorCode::INVALID_ARGUMENT,
                                         .message = "Environment radiance must be finite and nonnegative"};
                }
            }
        }
        return result;
    }
} // namespace sky::aurora
