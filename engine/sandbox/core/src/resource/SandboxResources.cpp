//
// Created on 2026/09/22.
//

#include <editor/core/resource/SandboxResources.h>

#include <framework/platform/PlatformBase.h>

#include <filesystem>

namespace sky::editor {

    std::string SandboxResources::Root()
    {
        std::string base = Platform::Get()->GetBundlePath();
        if (!base.empty() && base.back() != '/' && base.back() != '\\') {
            base += '/';
        }
        return base + "resources/";
    }

    std::string SandboxResources::Resolve(const std::string &relative)
    {
        return Root() + relative;
    }

    std::string SandboxResources::CacheRoot()
    {
        const auto *platform = Platform::Get();
        const auto  base     = platform ? platform->GetUserConfigPath() : std::string{};
        return base.empty() ? std::string{} : (std::filesystem::path(base) / "skyengine" / "cache" / "editor").string();
    }

} // namespace sky::editor
