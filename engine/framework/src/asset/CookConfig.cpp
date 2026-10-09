//
// Created by blues on 2026/10/2.
//

#include <framework/asset/CookConfig.h>

#include <core/logger/Logger.h>

#include <rapidjson/document.h>

static const char *TAG = "CookConfig";

namespace sky {

    namespace {

        // Scalar -> string for the cook override transport. Returns false for non-scalars
        // (objects/arrays/null), which are ignored.
        bool ScalarToString(const rapidjson::Value &value, std::string &out)
        {
            if (value.IsBool()) {
                out = value.GetBool() ? "true" : "false";
                return true;
            }
            if (value.IsInt()) {
                out = std::to_string(value.GetInt());
                return true;
            }
            if (value.IsUint()) {
                out = std::to_string(value.GetUint());
                return true;
            }
            if (value.IsInt64()) {
                out = std::to_string(value.GetInt64());
                return true;
            }
            if (value.IsUint64()) {
                out = std::to_string(value.GetUint64());
                return true;
            }
            if (value.IsDouble()) {
                out = std::to_string(value.GetDouble());
                return true;
            }
            if (value.IsString()) {
                out = value.GetString();
                return true;
            }
            return false;
        }

        std::vector<std::string> ParseAssetTargets(const std::string &assetCookJson)
        {
            std::vector<std::string> result;
            if (assetCookJson.empty()) {
                return result;
            }

            rapidjson::Document cook;
            cook.Parse<rapidjson::kParseCommentsFlag>(assetCookJson.c_str(), assetCookJson.size());
            if (!cook.HasParseError() && cook.IsObject() && cook.HasMember("targets") && cook["targets"].IsArray()) {
                for (const auto &target : cook["targets"].GetArray()) {
                    if (target.IsString()) {
                        result.emplace_back(target.GetString());
                    }
                }
            }
            return result;
        }

    } // namespace

    bool CookConfig::Parse(const std::string &text)
    {
        rapidjson::Document doc;
        doc.Parse<rapidjson::kParseCommentsFlag>(text.c_str(), text.size());
        if (doc.HasParseError() || !doc.IsObject()) {
            LOG_W(TAG, "Failed to parse cook config");
            return false;
        }

        platformTargets.clear();
        targets.clear();
        bundles.clear();
        presets.clear();
        mode = CookMode::InProcess;
        workerPath.clear();
        workerTimeoutMs = 10u * 60u * 1000u;

        if (doc.HasMember("cook") && doc["cook"].IsObject()) {
            const auto &cook = doc["cook"];
            if (cook.HasMember("mode") && cook["mode"].IsString()) {
                const std::string name = cook["mode"].GetString();
                mode                   = (name == "out-of-process") ? CookMode::OutOfProcess : CookMode::InProcess;
            }
            if (cook.HasMember("worker") && cook["worker"].IsObject()) {
                const auto &worker = cook["worker"];
                if (worker.HasMember("path") && worker["path"].IsString()) {
                    workerPath = worker["path"].GetString();
                }
                if (worker.HasMember("timeoutMs") && worker["timeoutMs"].IsUint()) {
                    workerTimeoutMs = worker["timeoutMs"].GetUint();
                }
            }
        }

        if (doc.HasMember("platforms") && doc["platforms"].IsObject()) {
            for (auto iter = doc["platforms"].MemberBegin(); iter != doc["platforms"].MemberEnd(); ++iter) {
                if (iter->value.IsString()) {
                    platformTargets[iter->name.GetString()] = iter->value.GetString();
                }
            }
        }

        if (doc.HasMember("targets") && doc["targets"].IsObject()) {
            for (auto iter = doc["targets"].MemberBegin(); iter != doc["targets"].MemberEnd(); ++iter) {
                if (!iter->value.IsObject()) {
                    continue;
                }

                CookTarget target;
                if (iter->value.HasMember("bundle") && iter->value["bundle"].IsString()) {
                    target.bundle = iter->value["bundle"].GetString();
                }
                targets[iter->name.GetString()] = std::move(target);
            }
        }

        if (doc.HasMember("bundles") && doc["bundles"].IsArray()) {
            for (const auto &bundle : doc["bundles"].GetArray()) {
                if (bundle.IsString()) {
                    bundles.emplace_back(bundle.GetString());
                }
            }
        }

        if (doc.HasMember("presets") && doc["presets"].IsObject()) {
            for (auto iter = doc["presets"].MemberBegin(); iter != doc["presets"].MemberEnd(); ++iter) {
                if (!iter->value.IsArray()) {
                    continue;
                }
                auto &list = presets[iter->name.GetString()];
                for (const auto &bundle : iter->value.GetArray()) {
                    if (bundle.IsString()) {
                        list.emplace_back(bundle.GetString());
                    }
                }
            }
        }
        return true;
    }

    std::vector<std::string> CookConfig::GetPresetBundles(const std::string &platform) const
    {
        auto iter = presets.find(platform);
        return iter != presets.end() ? iter->second : std::vector<std::string>{};
    }

    std::string CookConfig::ResolveTargetForPlatform(const std::string &platform) const
    {
        auto iter = platformTargets.find(platform);
        return iter != platformTargets.end() ? iter->second : std::string{};
    }

    const CookTarget *CookConfig::FindTarget(const std::string &name) const
    {
        auto iter = targets.find(name);
        return iter != targets.end() ? &iter->second : nullptr;
    }

    std::string CookConfig::ResolveTarget(const std::string &assetCookJson) const
    {
        auto assetTargets = ParseAssetTargets(assetCookJson);
        if (!assetTargets.empty()) {
            return assetTargets.front();
        }

        auto resolved = ResolveTargetForPlatform(activePlatform);
        return resolved.empty() ? std::string("common") : resolved;
    }

    std::vector<std::string> CookConfig::GetTargets(const std::string &assetCookJson) const
    {
        return GetTargets(assetCookJson, activePlatform);
    }

    std::vector<std::string> CookConfig::GetTargets(const std::string &assetCookJson, const std::string &platform) const
    {
        auto result = ParseAssetTargets(assetCookJson);
        if (result.empty()) {
            for (const auto &[name, target] : targets) {
                result.push_back(name);
            }
        }
        if (result.empty()) {
            // No asset/project targets: fall back to the platform's preset product bundles.
            result = GetPresetBundles(platform);
        }
        return result;
    }

    std::string CookConfig::ResolveBundleForTarget(const std::string &target) const
    {
        const CookTarget *found = FindTarget(target);
        return found != nullptr ? found->bundle : target;
    }

    std::map<std::string, std::string> CookConfig::GetTargetSettings(const std::string &assetCookJson, const std::string &target) const
    {
        std::map<std::string, std::string> result;
        if (assetCookJson.empty() || target.empty()) {
            return result;
        }

        rapidjson::Document cook;
        cook.Parse<rapidjson::kParseCommentsFlag>(assetCookJson.c_str(), assetCookJson.size());
        if (cook.HasParseError() || !cook.IsObject() || !cook.HasMember("settings") || !cook["settings"].IsObject()) {
            return result;
        }

        const auto &settings = cook["settings"];
        if (!settings.HasMember(target.c_str()) || !settings[target.c_str()].IsObject()) {
            return result;
        }

        for (auto iter = settings[target.c_str()].MemberBegin(); iter != settings[target.c_str()].MemberEnd(); ++iter) {
            std::string value;
            if (iter->name.IsString() && ScalarToString(iter->value, value)) {
                result[iter->name.GetString()] = std::move(value);
            }
        }
        return result;
    }

} // namespace sky
