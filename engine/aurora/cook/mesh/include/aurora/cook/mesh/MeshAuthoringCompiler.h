#pragma once

#include <memory>

namespace sky::phy {
    class IMeshAuthoringCompiler;
}
namespace sky::aurora::cook {
    struct MeshBuildPresets;
    std::shared_ptr<phy::IMeshAuthoringCompiler> CreateMeshAuthoringCompiler(std::shared_ptr<const MeshBuildPresets> presets = {});
} // namespace sky::aurora::cook
