#pragma once

#include <aurora/pipeline/GlobalVariantLayout.h>
#include <aurora/rhi/Shader.h>
#include <aurora/shader/ShaderVariant.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky::aurora {

    struct ShaderVariantInfo {
        ShaderVariantKey     key;
        bool hasPackedKey = false;
        ShaderVariantSelection selection;
        ShaderVariant        values; // Complete normalized V/P/S values.
        ShaderSpecialization spec;
        uint64_t             variantHash = 0;
    };

    struct ShaderVariantInputs {
        const GlobalVariantLayout           *global     = nullptr;
        const ShaderVariantSchema           *schema     = nullptr; // per-shader
        const std::vector<VertexVariantDef> *vertexDefs = nullptr;
        VertexSemanticMask                   vertexMask{};
        // Pipeline-owned values (P region). Only keys declared by `global`.
        const ShaderVariant *pipelineValues = nullptr;
        // Shader-local values supplied by Material, Batch, Fullscreen or Compute.
        const ShaderVariant *shaderValues = nullptr;
    };

    bool BuildShaderVariant(const ShaderVariantInputs &inputs, ShaderVariantInfo &out, std::string *error = nullptr);

} // namespace sky::aurora
