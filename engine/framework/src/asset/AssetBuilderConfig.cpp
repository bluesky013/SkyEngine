//
// Created by blues on 2024/7/21.
//

#include <framework/asset/AssetBuilderConfig.h>

namespace sky {

    void AssetBuilderConfig::LoadJson(JsonInputArchive &json)
    {
        uint32_t count = json.StartArray("bundles");
        for (uint32_t i = 0; i < count; ++i) {
            bundles.emplace_back(json.LoadString());
            json.NextArrayElement();
        }
        json.End();

        json.Start("presets");
        json.ForEachMember([&json, this](const std::string &key) {
            uint32_t count = json.StartArray(key);
            auto &preset = presets[key];
            for (uint32_t i = 0; i < count; ++i) {
                preset.emplace_back(json.LoadString());
                json.NextArrayElement();
            }

            json.End();
        });

        json.End();
    }

    void AssetBuilderConfig::SaveJson(JsonOutputArchive &archive)
    {

    }

} // namespace sky