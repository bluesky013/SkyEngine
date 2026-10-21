#pragma once

#include <aurora/renderer/RenderRasterData.h>
#include <aurora/renderer/RendererTypes.h>
#include <aurora/shader/ShaderRef.h>
#include <aurora/shader/ShaderVariant.h>

namespace sky {
    class IFileSystem;
}
namespace sky::aurora {
    class ShaderFileSystem;
    struct ShaderTechnique {
        ShaderRef     vertex, fragment, compute;
        ShaderVariant variant;
    };
    // Exactly vertex + fragment or compute; entry names and source paths are explicit.
    std::variant<RendererError, RenderTechniqueData>
    CompileShaderTechnique(ShaderFileSystem &, API, const ShaderTechnique &, sky::IFileSystem *products = nullptr);
} // namespace sky::aurora
