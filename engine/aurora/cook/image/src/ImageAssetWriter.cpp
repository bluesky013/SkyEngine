//
// ImageAssetData output mapping (see ImageAssetWriter.h).
//

#include <aurora/cook/image/ImageAssetWriter.h>

namespace sky::aurora::cook {

    namespace {

        void AppendSlice(ImageAssetData &out, const uint8_t *ptr, uint32_t size, uint32_t mip, uint32_t layer, uint32_t depth)
        {
            ImageSliceHeader slice = {};
            slice.offset   = static_cast<uint32_t>(out.rawData.size());
            slice.size     = size;
            slice.mipLevel = mip;
            slice.layer    = layer;
            slice.depth    = depth;
            out.slices.push_back(slice);

            out.rawData.insert(out.rawData.end(), ptr, ptr + size);
        }

    } // namespace

    void WriteImageAsset(const ImageObject &image, ImageAssetType type, ImageAssetData &out)
    {
        out.clear();
        out.format      = image.format;
        out.type        = type;
        out.width       = image.width;
        out.height      = image.height;
        out.depth       = type == ImageAssetType::TEXTURE_3D ? image.depth : 1;
        out.mipLevels   = static_cast<uint32_t>(image.mips.size());
        out.arrayLayers = (type == ImageAssetType::TEXTURE_CUBE || type == ImageAssetType::TEXTURE_2D_ARRAY) ? image.depth : 1;

        for (uint32_t mip = 0; mip < image.mips.size(); ++mip) {
            const auto    &mipData   = image.mips[mip];
            const uint32_t layerSize = mipData.width * mipData.height * image.pixelSize;

            if (type == ImageAssetType::TEXTURE_3D) {
                AppendSlice(out, mipData.data.get(), layerSize * image.depth, mip, 0, 0);
            } else {
                for (uint32_t layer = 0; layer < image.depth; ++layer) {
                    AppendSlice(out, mipData.data.get() + layer * layerSize, layerSize, mip, layer, 0);
                }
            }
        }
    }

    void WriteImageAsset(const CompressedImage &image, ImageAssetType type, ImageAssetData &out)
    {
        out.clear();
        out.format      = image.format;
        out.type        = type;
        out.width       = image.width;
        out.height      = image.height;
        out.depth       = 1;
        out.mipLevels   = static_cast<uint32_t>(image.mips.size());
        out.arrayLayers = 1;

        for (uint32_t mip = 0; mip < image.mips.size(); ++mip) {
            const auto &mipData = image.mips[mip];
            AppendSlice(out, mipData.data.get(), mipData.dataLength, mip, 0, 0);
        }
    }

} // namespace sky::aurora::cook
