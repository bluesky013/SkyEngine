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
        ShaderVariant variant; // Shader-local values.
    };
    // Fully composed P|V|S selection. Callers own the domain split (pipeline,
    // vertex, material); this type only carries the normalized result.
    struct ShaderTechniqueVariant {
        ShaderVariant        macros;
        VertexSemanticMask   vertexFeatures;
        uint64_t             variantHash = 0;
        ShaderVariantKey     key;
        std::string          dump;
        ShaderVariantSchema  schema; // combined P|S schema used for validation
    };
    // Exactly vertex + fragment or compute; entry names and source paths are explicit.
    // When `resolved` is null, `technique.variant` supplies S values; V is empty and P uses defaults.
    std::variant<RendererError, RenderTechniqueData> CompileShaderTechnique(ShaderFileSystem &,
                                                                           API,
                                                                           const ShaderTechnique           &,
                                                                           sky::IFileSystem                *products = nullptr,
                                                                           const ShaderTechniqueVariant    *resolved = nullptr);
} // namespace sky::aurora
