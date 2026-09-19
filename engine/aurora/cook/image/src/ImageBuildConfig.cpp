//
// Image cook configuration parsing (see ImageBuildConfig.h).
//

#include <aurora/cook/image/ImageBuildConfig.h>

#include <core/logger/Logger.h>

#include <string>

static const char *TAG = "AuroraImageCook";

namespace sky::aurora::cook {

    namespace {

        ImageEncode ParseEncode(const std::string &value)
        {
            if (value == "BC7") {
                return ImageEncode::BC7;
            }
            if (value == "ASTC") {
                return ImageEncode::ASTC;
            }
            return ImageEncode::NONE;
        }

        Quality ParseQuality(const std::string &value)
        {
            if (value == "ULTRA_FAST") {
                return Quality::ULTRA_FAST;
            }
            if (value == "VERY_FAST") {
                return Quality::VERY_FAST;
            }
            if (value == "FAST") {
                return Quality::FAST;
            }
            if (value == "BASIC") {
                return Quality::BASIC;
            }
            if (value == "SLOW") {
                return Quality::SLOW;
            }
            return Quality::FAST;
        }

    } // namespace

    PixelFormat ImageBuildConfig::ResolveFormat() const
    {
        switch (encode) {
        case ImageEncode::BC7:
            return srgb ? PixelFormat::BC7_SRGB_BLOCK : PixelFormat::BC7_UNORM_BLOCK;
        case ImageEncode::ASTC:
            if (astcBlock == 8) {
                return srgb ? PixelFormat::ASTC_8x8_SRGB_BLOCK : PixelFormat::ASTC_8x8_UNORM_BLOCK;
            }
            return srgb ? PixelFormat::ASTC_4x4_SRGB_BLOCK : PixelFormat::ASTC_4x4_UNORM_BLOCK;
        case ImageEncode::NONE:
        default:
            return srgb ? PixelFormat::RGBA8_SRGB : PixelFormat::RGBA8_UNORM;
        }
    }

    void ImageBuildPresets::LoadJson(JsonInputArchive &json)
    {
        if (json.Start("defaultBundle")) {
            defaultBundle = json.LoadString();
            json.End();
        }

        if (!json.Start("bundles")) {
            return;
        }

        json.ForEachMember([&json, this](const std::string &key) {
            ImageBuildConfig cfg;
            json.Start(key);

            if (json.Start("encode")) {
                cfg.encode = ParseEncode(json.LoadString());
                json.End();
            }
            if (json.Start("quality")) {
                cfg.quality = ParseQuality(json.LoadString());
                json.End();
            }
            if (json.Start("srgb")) {
                cfg.srgb = json.LoadBool();
                json.End();
            }
            if (json.Start("block")) {
                cfg.astcBlock = json.LoadUint();
                json.End();
            }
            if (json.Start("maxSize")) {
                cfg.maxSize = json.LoadUint();
                json.End();
            }
            if (json.Start("generateMip")) {
                cfg.generateMip = json.LoadBool();
                json.End();
            }

            json.End();
            bundles[key] = cfg;
        });

        json.End();
    }

    const ImageBuildConfig *ImageBuildPresets::Resolve(const std::string &requested, std::string &resolvedKey) const
    {
        if (!requested.empty()) {
            auto iter = bundles.find(requested);
            if (iter != bundles.end()) {
                resolvedKey = iter->first;
                return &iter->second;
            }
            LOG_W(TAG, "unknown image bundle '%s', falling back to '%s'", requested.c_str(), defaultBundle.c_str());
        }

        auto iter = bundles.find(defaultBundle);
        if (iter != bundles.end()) {
            resolvedKey = iter->first;
            return &iter->second;
        }
        return nullptr;
    }

} // namespace sky::aurora::cook
