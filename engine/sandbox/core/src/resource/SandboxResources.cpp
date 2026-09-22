//
// Created on 2026/09/22.
//

#include <editor/core/resource/SandboxResources.h>

#include <framework/platform/PlatformBase.h>

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

} // namespace sky::editor
