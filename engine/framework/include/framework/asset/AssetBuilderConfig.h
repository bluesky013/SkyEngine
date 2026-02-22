//
// Created by blues on 2024/7/21.
//

#pragma once

#include <vector>
#include <string>
#include <map>
#include <core/platform/Platform.h>
#include <framework/serialization/JsonArchive.h>

namespace sky {

    struct AssetBuilderConfig {
        std::vector<std::string> bundles;
        std::map<std::string, std::vector<std::string>> presets;

        void LoadJson(JsonInputArchive &archive);
        void SaveJson(JsonOutputArchive &archive);
    };

} // namespace sky
