//
// Created on 2026/11/10.
//

#pragma once

#include <framework/application/ModuleManager.h>
#include <string_view>
#include <variant>

namespace sky {
    using ModuleBuildOptionLookup = std::optional<bool> (*)(std::string_view);
    using ModuleManifestResult    = std::variant<std::string, std::vector<ModuleInfo>>;

    // Parses the complete selection before publishing it; file lookup remains host-owned.
    ModuleManifestResult ParseModuleManifest(std::string_view json, ModuleBuildOptionLookup options = ModuleManager::GetBuildOption);
} // namespace sky
