//
// ShaderVariantAssembler: composes the 128-bit shader variant key from the
// pipeline layout, vertex semantics, and the per-shader schema (material
// overrides / defaults), producing the variant key + specialization + hash.
//
// Region layout (see GlobalVariantLayout): [pipeline P | vertex V | shader S].
//

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
        ShaderSpecialization spec;
        uint64_t             variantHash = 0;
    };

    struct ShaderVariantInputs {
        const GlobalVariantLayout           *global     = nullptr;
        const ShaderVariantSchema           *schema     = nullptr; // per-shader
        const std::vector<VertexVariantDef> *vertexDefs = nullptr;
        VertexSemanticMask                   vertexMask{};
        const ShaderVariant                 *overrides = nullptr; // material Name->value
    };

    // Assemble the variant key + specialization + hash. v1 folds spec values into
    // variantHash (no moduleKey / psoKey split yet).
    bool BuildShaderVariant(const ShaderVariantInputs &inputs, ShaderVariantInfo &out, std::string *error = nullptr);

} // namespace sky::aurora
