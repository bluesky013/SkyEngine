#pragma once

#include <aurora/rhi/ShaderParameters.h>
#include <aurora/shader/ShaderVariant.h>

namespace sky::aurora {
    enum class MaterialKeyKind : uint8_t { FIELD, TEXTURE, SAMPLER, OPTION };
    struct MaterialKey {
        std::string       name;
        std::string       owner;
        MaterialKeyKind   kind       = MaterialKeyKind::FIELD;
        ShaderDataType    dataType   = ShaderDataType::UNKNOWN;
        uint32_t          arrayCount = 0;
        ShaderTextureType texture;
        ShaderSamplerType sampler                               = ShaderSamplerType::UNKNOWN;
        ShaderVariantType optionType                            = ShaderVariantType::UINT32;
        uint8_t           optionWidth                           = 0;
        uint32_t          optionDefault                         = 0;
        bool              operator==(const MaterialKey &) const = default;
    };
    struct MaterialSchema {
        static constexpr uint32_t kVersion = 1;
        std::vector<MaterialKey>  keys;
    };

    bool BuildMaterialSchema(const ShaderReflection &reflection, const ShaderVariantSchema &options, MaterialSchema &out, std::string &error);
    bool MergeMaterialSchemas(const MaterialSchema &input, MaterialSchema &output, std::string &error);
} // namespace sky::aurora
