//
// Aurora image builder (see AuroraImageBuilder.h).
//

#include <aurora/cook/image/AuroraImageBuilder.h>

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/cook/image/ImageAssetWriter.h>
#include <aurora/cook/image/ImageCompressor.h>
#include <aurora/cook/image/ImageConverter.h>
#include <aurora/cook/image/ImageMipGen.h>
#include <aurora/cook/image/ImageResizer.h>
#include <aurora/cook/image/ImageSource.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>

#include <core/logger/Logger.h>
#include <core/platform/Platform.h>

#include <string>
#include <vector>

static const char *TAG = "AuroraImageBuilder";

namespace sky::aurora {

    namespace {

        bool DetectAlpha(const cook::ImageObject &image)
        {
            if (image.mips.empty() || image.pixelSize != 4) {
                return false;
            }
            const auto    &mip  = image.mips[0];
            const uint8_t *data = mip.data.get();
            const uint32_t count = mip.width * mip.height;
            for (uint32_t i = 0; i < count; ++i) {
                if (data[i * 4 + 3] != 255) {
                    return true;
                }
            }
            return false;
        }

    } // namespace

    std::string_view AuroraImageBuilder::QueryType(const std::string &) const
    {
        return AssetTraits<Texture>::ASSET_TYPE;
    }

    void AuroraImageBuilder::LoadConfig(const FileSystemPtr &cfg)
    {
        if (cfg == nullptr) {
            return;
        }
        auto file = cfg->OpenFile(FilePath("image_build_presets.json"));
        if (!file) {
            LOG_W(TAG, "image_build_presets.json not found; using fallback config");
            return;
        }

        auto archive = file->ReadAsArchive();
        JsonInputArchive json(*archive);
        presets.LoadJson(json);
        LOG_I(TAG, "loaded %u image build bundles (default '%s')", static_cast<uint32_t>(presets.bundles.size()), presets.defaultBundle.c_str());
    }

    void AuroraImageBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;

        std::string             bundleKey;
        const cook::ImageBuildConfig *config = presets.Resolve(request.target, bundleKey);
        if (config == nullptr) {
            LOG_E(TAG, "no image build config for target '%s'", request.target.c_str());
            return;
        }

        std::vector<uint8_t> bytes;
        if (!request.file->ReadBin(bytes) || bytes.empty()) {
            LOG_E(TAG, "failed to read source %s", request.assetInfo->path.path.GetStr().c_str());
            return;
        }

        auto *manager = AssetManager::Get();
        auto  asset   = manager->FindOrCreateAsset<Texture>(request.assetInfo->uuid);
        if (!asset) {
            LOG_E(TAG, "failed to create texture asset %s", request.assetInfo->uuid.ToString().c_str());
            return;
        }

        const std::string &ext = request.assetInfo->ext;
        auto              &imageData = asset->Data();

        // 1) Source decode.
        cook::CookImageSource source;
        if (ext == ".ktx") {
            if (!cook::LoadKtx(bytes, source)) {
                LOG_E(TAG, "ktx decode failed: %s", request.assetInfo->path.path.GetStr().c_str());
                return;
            }
        } else if (ext == ".image") {
            // Re-cook of an aurora product: skip the asset header (type + deps)
            // and read the ImageAssetData payload.
            auto stream = request.file->ReadAsArchive();
            std::string type;
            stream->Load(type);
            uint32_t depCount = 0;
            stream->Load(depCount);
            for (uint32_t i = 0; i < depCount; ++i) {
                uint32_t a = 0;
                uint32_t b = 0;
                stream->Load(a);
                stream->Load(b);
            }
            BinaryInputArchive bin(*stream);
            imageData.Load(bin);
            manager->SaveAsset(asset, bundleKey);
            result.retCode = AssetBuildRetCode::SUCCESS;
            return;
        } else {
            auto image = cook::LoadStbImage(bytes, ext == ".hdr");
            if (!image) {
                LOG_E(TAG, "image decode failed: %s", request.assetInfo->path.path.GetStr().c_str());
                return;
            }
            source.image     = image;
            source.assetType = ImageAssetType::TEXTURE_2D;
        }

        // 2) Already block compressed (KTX with BC/ASTC payload): pass through.
        if (source.precompressed) {
            imageData = source.asset;
            manager->SaveAsset(asset, bundleKey);
            result.retCode = AssetBuildRetCode::SUCCESS;
            return;
        }

        auto image = source.image;
        const ImageAssetType assetType = source.assetType;

        // 3) Limit resolution.
        if (config->maxSize > 0) {
            cook::ImageResizer::Payload resize;
            resize.image     = image;
            resize.maxWidth  = config->maxSize;
            resize.maxHeight = config->maxSize;
            cook::ImageResizer(resize).DoWork();
        }

        // 4) Linearize -> mip chain -> encode back to the source space.
        //    Compression is only wired for 2D; cube/array/3D stay uncompressed.
        if (config->generateMip) {
            auto linear = cook::ImageObject::CreateImage2D(image->width, image->height, image->format);
            linear->depth = image->depth;
            linear->FillMip0();

            cook::ImageConverter::Payload toLinear;
            toLinear.src   = image;
            toLinear.dst   = linear;
            toLinear.gamma = config->srgb ? 2.2f : 1.f;
            cook::ImageConverter(toLinear).DoWork();

            cook::ImageMipGen::Payload mipGen;
            mipGen.image = linear;
            mipGen.type  = cook::MipGenType::Kaiser;
            cook::ImageMipGen(mipGen).DoWork();

            auto finalImage = cook::ImageObject::CreateFromImage(linear);
            cook::ImageConverter::Payload toSrgb;
            toSrgb.src   = linear;
            toSrgb.dst   = finalImage;
            toSrgb.gamma = config->srgb ? 1.f / 2.2f : 1.f;
            cook::ImageConverter(toSrgb).DoWork();

            image = finalImage;
        }

        // 5) Compress / emit.
        if (config->IsCompressed() && assetType == ImageAssetType::TEXTURE_2D) {
            const bool hasAlpha = DetectAlpha(*image);
            auto compressed = cook::CompressedImage::CreateFromImageObject(image, config->ResolveFormat());
            for (uint32_t mip = 0; mip < image->mips.size(); ++mip) {
                cook::ImageCompressor::Payload compress;
                compress.image      = image;
                compress.compressed = compressed;
                compress.config     = *config;
                compress.mip        = mip;
                compress.hasAlpha   = hasAlpha;
                cook::ImageCompressor(compress).DoWork();
            }
            cook::WriteImageAsset(*compressed, assetType, imageData);
        } else {
            cook::WriteImageAsset(*image, assetType, imageData);
        }

        manager->SaveAsset(asset, bundleKey);
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::aurora
