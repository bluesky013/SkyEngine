//
// CreateTextureFromAsset: runtime-only build from ImageAssetData to a
// dimension-specific aurora::Texture (2D / 2D array / 3D / cube).
//

#include <aurora/adaptor/assets/ImageAsset.h>

#include <aurora/resource/Buffer.h>
#include <core/logger/Logger.h>

#include <algorithm>
#include <vector>

static const char *TAG = "AuroraImageAsset";

namespace sky::aurora {

    namespace {

        Extent3D MipExtent(const ImageAssetData &data, uint32_t mipLevel)
        {
            const uint32_t shift = mipLevel;
            Extent3D       extent = {};
            extent.width         = std::max(1u, data.width >> shift);
            extent.height        = std::max(1u, data.height >> shift);
            extent.depth         = data.type == ImageAssetType::TEXTURE_3D ? std::max(1u, data.depth >> shift) : 1u;
            return extent;
        }

    } // namespace

    CounterPtr<Texture> CreateTextureFromAsset(Device *device, const Asset<Texture> &asset)
    {
        if (device == nullptr) {
            return {};
        }
        const ImageAssetData &data = asset.Data();
        if (data.version != ImageAssetData::CURRENT_VERSION) {
            LOG_E(TAG, "unsupported image asset version: %u (expected %u)", data.version, ImageAssetData::CURRENT_VERSION);
            return {};
        }
        if (data.format == PixelFormat::UNDEFINED || data.width == 0 || data.height == 0) {
            LOG_E(TAG, "invalid image asset (format/size)");
            return {};
        }

        Image::Descriptor desc = {};
        desc.format            = data.format;
        desc.mipLevels         = data.mipLevels;
        desc.samples           = SampleCount::X1;
        desc.usage             = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
        desc.memory            = MemoryType::GPU_ONLY;

        CounterPtr<Texture> texture;
        switch (data.type) {
        case ImageAssetType::TEXTURE_2D:
            desc.imageType   = ImageType::IMAGE_2D;
            desc.extent      = {data.width, data.height, 1};
            desc.arrayLayers = 1;
            texture          = new Texture2D();
            break;
        case ImageAssetType::TEXTURE_2D_ARRAY:
            desc.imageType   = ImageType::IMAGE_2D;
            desc.extent      = {data.width, data.height, 1};
            desc.arrayLayers = data.arrayLayers;
            texture          = new Texture2DArray();
            break;
        case ImageAssetType::TEXTURE_3D:
            desc.imageType   = ImageType::IMAGE_3D;
            desc.extent      = {data.width, data.height, data.depth};
            desc.arrayLayers = 1;
            texture          = new Texture3D();
            break;
        case ImageAssetType::TEXTURE_CUBE:
            if (data.arrayLayers != 6) {
                LOG_E(TAG, "cube image asset requires 6 layers, got %u", data.arrayLayers);
                return {};
            }
            desc.imageType   = ImageType::IMAGE_2D;
            desc.extent      = {data.width, data.height, 1};
            desc.arrayLayers = 6;
            desc.viewUsage   = ImageViewUsageFlagBit::CUBE_MAP_COMPATIBLE;
            texture          = new TextureCube();
            break;
        default:
            LOG_E(TAG, "unknown image asset type: %u", static_cast<uint32_t>(data.type));
            return {};
        }

        if (texture == nullptr || !texture->Init(device, desc)) {
            return {};
        }

        if (!data.slices.empty()) {
            // Keep the source streams alive until the transfer completes.
            std::vector<CounterPtr<RawBufferStream>> streams;
            std::vector<ImageUploadRequest>          requests;
            streams.reserve(data.slices.size());
            requests.reserve(data.slices.size());

            for (const auto &slice : data.slices) {
                const uint64_t end = static_cast<uint64_t>(slice.offset) + slice.size;
                if (end > data.rawData.size()) {
                    LOG_E(TAG, "image slice out of range (offset=%u size=%u)", slice.offset, slice.size);
                    return {};
                }
                auto *stream = new RawBufferStream(data.rawData.data() + slice.offset, slice.size);
                streams.emplace_back(stream);

                ImageUploadRequest request = {};
                request.source             = CounterPtr<IUploadStream>(stream);
                request.offset             = 0;
                request.size               = slice.size;
                request.mipLevel           = slice.mipLevel;
                request.layer              = slice.layer;
                request.imageExtent        = MipExtent(data, slice.mipLevel);
                requests.push_back(request);
            }

            if (!texture->UploadImage(requests)) {
                return {};
            }
            texture->WaitUploadComplete();
        } else if (!data.rawData.empty()) {
            texture->Upload(data.rawData.data(), data.rawData.size());
            texture->WaitUploadComplete();
        }

        return texture;
    }

} // namespace sky::aurora
