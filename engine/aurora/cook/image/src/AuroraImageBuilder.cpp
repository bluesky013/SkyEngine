//
// Aurora image builder (see AuroraImageBuilder.h).
//

#include <aurora/cook/image/AuroraImageBuilder.h>

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/cook/image/ImageAssetWriter.h>
#include <aurora/cook/image/ImageCompressor.h>
#include <aurora/cook/image/ImageConverter.h>
#include <aurora/cook/image/ImageCookSettings.h>
#include <aurora/cook/image/ImageMipGen.h>
#include <aurora/cook/image/ImageResizer.h>
#include <aurora/cook/image/ImageSource.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>

#include <core/logger/Logger.h>
#include <core/platform/Platform.h>
#include <core/type/TypeInfo.h>

#include <string>
#include <utility>
#include <vector>

static const char *TAG = "AuroraImageBuilder";

namespace sky::aurora {

    namespace {

        bool DetectAlpha(const cook::ImageObject &image)
        {
            if (image.mips.empty() || image.pixelSize != 4) {
                return false;
            }
            const auto    &mip   = image.mips[0];
            const uint8_t *data  = mip.data.get();
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

    namespace {

        const char *EncodeName(cook::ImageEncode encode)
        {
            switch (encode) {
            case cook::ImageEncode::BC7: return "BC7";
            case cook::ImageEncode::ASTC: return "ASTC";
            default: return "NONE";
            }
        }

        const char *QualityName(cook::Quality quality)
        {
            switch (quality) {
            case cook::Quality::ULTRA_FAST: return "ultra_fast";
            case cook::Quality::VERY_FAST: return "very_fast";
            case cook::Quality::FAST: return "fast";
            case cook::Quality::BASIC: return "basic";
            case cook::Quality::SLOW: return "slow";
            }
            return "fast";
        }

        std::vector<std::pair<std::string, std::string>> FormatSettings(const cook::ImageBuildConfig &config, const std::string &resolvedKey)
        {
            std::vector<std::pair<std::string, std::string>> out;
            out.emplace_back("bundle", resolvedKey);
            out.emplace_back("encode", EncodeName(config.encode));
            out.emplace_back("srgb", config.srgb ? "true" : "false");
            out.emplace_back("quality", QualityName(config.quality));
            out.emplace_back("astcBlock", std::to_string(config.astcBlock));
            out.emplace_back("maxSize", config.maxSize == 0 ? std::string("unlimited") : std::to_string(config.maxSize));
            out.emplace_back("generateMip", config.generateMip ? "true" : "false");
            return out;
        }

    } // namespace

    std::vector<std::pair<std::string, std::string>> AuroraImageBuilder::DescribeSettings(const ProductBundleKey &bundle) const
    {
        std::string                   resolvedKey;
        const cook::ImageBuildConfig *config = presets.Resolve(bundle, resolvedKey);
        if (config == nullptr) {
            return {};
        }
        return FormatSettings(*config, resolvedKey);
    }

    std::vector<std::pair<std::string, std::string>> AuroraImageBuilder::DescribeSettings(const ProductBundleKey      &bundle,
                                                                                          const BuildSettingsOverride &override) const
    {
        std::string                   resolvedKey;
        const cook::ImageBuildConfig *resolved = presets.Resolve(bundle, resolvedKey);
        if (resolved == nullptr) {
            return {};
        }
        cook::ImageBuildConfig config = *resolved;
        config.ApplyOverride(override);
        return FormatSettings(config, resolvedKey);
    }

    namespace {

        // Uppercase token matching ImageBuildConfig::ParseQuality().
        const char *QualityToken(cook::Quality quality)
        {
            switch (quality) {
            case cook::Quality::ULTRA_FAST: return "ULTRA_FAST";
            case cook::Quality::VERY_FAST: return "VERY_FAST";
            case cook::Quality::FAST: return "FAST";
            case cook::Quality::BASIC: return "BASIC";
            case cook::Quality::SLOW: return "SLOW";
            }
            return "FAST";
        }

    } // namespace

    AuroraImageBuilder::AuroraImageBuilder()
    {
        cook::RegisterImageCookSettings();
    }

    const TypeInfoRT *AuroraImageBuilder::GetSettingsType() const
    {
        cook::RegisterImageCookSettings();
        return TypeInfoObj<cook::ImageCookSettings>::Get()->RtInfo();
    }

    Any AuroraImageBuilder::MakeSettings(const ProductBundleKey &bundle, const BuildSettingsOverride &override) const
    {
        std::string                   resolvedKey;
        const cook::ImageBuildConfig *resolved = presets.Resolve(bundle, resolvedKey);
        if (resolved == nullptr) {
            return Any{};
        }
        cook::ImageBuildConfig config = *resolved;
        config.ApplyOverride(override);

        cook::ImageCookSettings settings;
        settings.encode      = config.encode;
        settings.srgb        = config.srgb;
        settings.quality     = config.quality;
        settings.block       = config.astcBlock;
        settings.maxSize     = config.maxSize;
        settings.generateMip = config.generateMip;
        return Any(settings);
    }

    BuildSettingsOverride AuroraImageBuilder::DiffSettings(const ProductBundleKey &bundle, const Any &edited) const
    {
        BuildSettingsOverride out;

        std::string                   resolvedKey;
        const cook::ImageBuildConfig *preset = presets.Resolve(bundle, resolvedKey);
        const auto                   *values = edited.GetAsConst<cook::ImageCookSettings>();
        if (preset == nullptr || values == nullptr) {
            return out;
        }

        if (values->encode != preset->encode) {
            out["encode"] = EncodeName(values->encode);
        }
        if (values->srgb != preset->srgb) {
            out["srgb"] = values->srgb ? "true" : "false";
        }
        if (values->quality != preset->quality) {
            out["quality"] = QualityToken(values->quality);
        }
        if (values->block != preset->astcBlock) {
            out["block"] = std::to_string(values->block);
        }
        if (values->maxSize != preset->maxSize) {
            out["maxSize"] = std::to_string(values->maxSize);
        }
        if (values->generateMip != preset->generateMip) {
            out["generateMip"] = values->generateMip ? "true" : "false";
        }
        return out;
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

        auto             archive = file->ReadAsArchive();
        JsonInputArchive json(*archive);
        presets.LoadJson(json);
        LOG_I(TAG, "loaded %u image build bundles (default '%s')", static_cast<uint32_t>(presets.bundles.size()), presets.defaultBundle.c_str());
    }

    namespace {

        void ResizeToLimit(const cook::ImageObjectPtr &image, uint32_t maxSize)
        {
            if (maxSize == 0) {
                return;
            }
            const uint32_t              beforeW = image->width;
            const uint32_t              beforeH = image->height;
            cook::ImageResizer::Payload resize;
            resize.image     = image;
            resize.maxWidth  = maxSize;
            resize.maxHeight = maxSize;
            cook::ImageResizer(resize).DoWork();
            LOG_I(TAG, "  resize: %ux%u -> %ux%u (max %u)", beforeW, beforeH, image->width, image->height, maxSize);
        }

        cook::ImageObjectPtr GenerateMipChain(const cook::ImageObjectPtr &image, bool srgb)
        {
            auto linear   = cook::ImageObject::CreateImage2D(image->width, image->height, image->format);
            linear->depth = image->depth;
            linear->FillMip0();

            cook::ImageConverter::Payload toLinear;
            toLinear.src   = image;
            toLinear.dst   = linear;
            toLinear.gamma = srgb ? 2.2f : 1.f;
            cook::ImageConverter(toLinear).DoWork();

            cook::ImageMipGen::Payload mipGen;
            mipGen.image = linear;
            mipGen.type  = cook::MipGenType::Kaiser;
            cook::ImageMipGen(mipGen).DoWork();

            auto                          finalImage = cook::ImageObject::CreateFromImage(linear);
            cook::ImageConverter::Payload toSrgb;
            toSrgb.src   = linear;
            toSrgb.dst   = finalImage;
            toSrgb.gamma = srgb ? 1.f / 2.2f : 1.f;
            cook::ImageConverter(toSrgb).DoWork();

            LOG_I(TAG, "  mipgen: %u levels (%ux%u)", static_cast<uint32_t>(finalImage->mips.size()), finalImage->width, finalImage->height);
            return finalImage;
        }

        void CompressAndEmit(const cook::ImageObjectPtr &image, const cook::ImageBuildConfig &config, ImageAssetType assetType, ImageAssetData &out)
        {
            const PixelFormat pixelFormat = cook::ResolveImageFormat(config);
            if (config.IsCompressed() && assetType == ImageAssetType::TEXTURE_2D) {
                const bool hasAlpha   = DetectAlpha(*image);
                auto       compressed = cook::CompressedImage::CreateFromImageObject(image, pixelFormat);
                for (uint32_t mip = 0; mip < image->mips.size(); ++mip) {
                    cook::ImageCompressor::Payload compress;
                    compress.image      = image;
                    compress.compressed = compressed;
                    compress.config     = config;
                    compress.mip        = mip;
                    compress.hasAlpha   = hasAlpha;
                    cook::ImageCompressor(compress).DoWork();
                }
                LOG_I(TAG, "  compress: format=%u mips=%u alpha=%d", static_cast<uint32_t>(pixelFormat), static_cast<uint32_t>(image->mips.size()),
                      hasAlpha ? 1 : 0);
                cook::WriteImageAsset(*compressed, assetType, out);
            } else {
                LOG_I(TAG, "  emit uncompressed: format=%u mips=%u", static_cast<uint32_t>(pixelFormat), static_cast<uint32_t>(image->mips.size()));
                cook::WriteImageAsset(*image, assetType, out);
            }
        }

    } // namespace

    void AuroraImageBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;

        std::string                   bundleKey;
        const std::string            &resolveKey = request.bundle.empty() ? request.target : request.bundle;
        const cook::ImageBuildConfig *resolved   = presets.Resolve(resolveKey, bundleKey);
        if (resolved == nullptr) {
            LOG_E(TAG, "no image build config for bundle '%s'", resolveKey.c_str());
            return;
        }

        // Overlay the per-asset sparse override on the bundle preset (unset keys keep the preset).
        cook::ImageBuildConfig config = *resolved;
        config.ApplyOverride(request.settings);

        LOG_I(TAG, "cook texture '%s': target='%s' bundle='%s' overrides=%u", request.assetInfo->path.GetStr().c_str(), request.target.c_str(),
              bundleKey.c_str(), static_cast<uint32_t>(request.settings.size()));
        LOG_I(TAG, "  config: encode=%s srgb=%d quality=%s block=%u maxSize=%u generateMip=%d", EncodeName(config.encode), config.srgb ? 1 : 0,
              QualityName(config.quality), config.astcBlock, config.maxSize, config.generateMip ? 1 : 0);

        std::vector<uint8_t> bytes;
        if (!request.file->ReadBin(bytes) || bytes.empty()) {
            LOG_E(TAG, "failed to read source %s", request.assetInfo->path.GetStr().c_str());
            return;
        }

        auto *manager = AssetManager::Get();
        auto  asset   = manager->FindOrCreateAsset<Texture>(request.assetInfo->uuid);
        if (!asset) {
            LOG_E(TAG, "failed to create texture asset %s", request.assetInfo->uuid.ToString().c_str());
            return;
        }

        const std::string &ext       = request.assetInfo->ext;
        auto              &imageData = asset->Data();

        // 1) Source decode.
        cook::CookImageSource source;
        if (ext == ".ktx") {
            if (!cook::LoadKtx(bytes, source)) {
                LOG_E(TAG, "ktx decode failed: %s", request.assetInfo->path.GetStr().c_str());
                return;
            }
            LOG_I(TAG, "  decode ktx: precompressed=%d bytes=%u", source.precompressed ? 1 : 0, static_cast<uint32_t>(bytes.size()));
        } else if (ext == ".image") {
            // Re-cook of an aurora product: skip the asset header (type + deps)
            // and read the ImageAssetData payload.
            auto        stream = request.file->ReadAsArchive();
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
            LOG_I(TAG, "  re-cook product passthrough: %s", request.assetInfo->path.GetStr().c_str());
            return;
        } else {
            auto image = cook::LoadStbImage(bytes, ext == ".hdr");
            if (!image) {
                LOG_E(TAG, "image decode failed: %s", request.assetInfo->path.GetStr().c_str());
                return;
            }
            source.image     = image;
            source.assetType = ImageAssetType::TEXTURE_2D;
            LOG_I(TAG, "  decode '%s': %ux%u", ext.c_str(), image->width, image->height);
        }

        // 2) Already block compressed (KTX with BC/ASTC payload): pass through.
        if (source.precompressed) {
            imageData = source.asset;
            manager->SaveAsset(asset, bundleKey);
            result.retCode = AssetBuildRetCode::SUCCESS;
            return;
        }

        cook::ImageObjectPtr image     = source.image;
        const ImageAssetType assetType = source.assetType;

        // 3) Limit resolution, (4) generate the mip chain, (5) compress / emit.
        ResizeToLimit(image, config.maxSize);
        if (config.generateMip) {
            image = GenerateMipChain(image, config.srgb);
        }
        CompressAndEmit(image, config, assetType, imageData);

        manager->SaveAsset(asset, bundleKey);
        result.retCode = AssetBuildRetCode::SUCCESS;
        LOG_I(TAG, "  saved texture '%s' bundle='%s'", request.assetInfo->path.GetStr().c_str(), bundleKey.c_str());
    }

} // namespace sky::aurora
