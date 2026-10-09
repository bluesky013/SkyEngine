//
// Image cook configuration parsing (see ImageBuildConfig.h).
//

#include <aurora/cook/image/ImageBuildConfig.h>

#include <core/logger/Logger.h>

#include <cstdlib>
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

        uint32_t ParseUint(const std::string &value, uint32_t fallback)
        {
            if (value.empty()) {
                return fallback;
            }
            char               *end    = nullptr;
            const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
            return end != value.c_str() ? static_cast<uint32_t>(parsed) : fallback;
        }

        bool ParseBool(const std::string &value, bool fallback)
        {
            if (value == "true" || value == "1") {
                return true;
            }
            if (value == "false" || value == "0") {
                return false;
            }
            return fallback;
        }

    } // namespace

    void ImageBuildConfig::ApplyOverride(const std::map<std::string, std::string> &override)
    {
        for (const auto &[key, value] : override) {
            if (key == "encode") {
                encode = ParseEncode(value);
            } else if (key == "quality") {
                quality = ParseQuality(value);
            } else if (key == "srgb") {
                srgb = ParseBool(value, srgb);
            } else if (key == "block") {
                astcBlock = ParseUint(value, astcBlock);
            } else if (key == "maxSize") {
                maxSize = ParseUint(value, maxSize);
            } else if (key == "generateMip") {
                generateMip = ParseBool(value, generateMip);
            }
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
