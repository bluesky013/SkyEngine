//
// ShaderCompile: backend-agnostic shader compile request/result types shared by
// the resolver and the (optional) compiler implementation.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/ShaderReflection.h>
#include <aurora/shader/ShaderVariant.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky::aurora {

    class ShaderFileSystem;

    enum class ShaderTarget : uint32_t {
        SPIRV = 0,
        MSL,
        DXIL,
    };

    struct ShaderCompileDesc {
        std::string                source;
        std::string                entry;
        ShaderStageFlagBit         stage;
        ShaderTarget               target     = ShaderTarget::SPIRV;
        ShaderFileSystem          *fileSystem = nullptr;
        const ShaderVariant       *variant    = nullptr;
        const ShaderVariantSchema *schema     = nullptr;
        ShaderCache               *cache      = nullptr;
    };

    struct ShaderCompileResult {
        std::vector<uint32_t> data; // SPIRV words; MSL text packed into words
        ShaderReflection      reflection;
        std::string           errorInfo;
    };

} // namespace sky::aurora
