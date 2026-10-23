//
// ShaderCompile: backend-agnostic shader compile request/result types shared by
// the resolver and the (optional) compiler implementation.
//

#pragma once

#include <aurora/rhi/ShaderArtifact.h>
#include <aurora/shader/ShaderVariant.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky::aurora {

    class ShaderFileSystem;

    struct ShaderCompileDesc {
        std::string                source;
        std::string                entry;
        ShaderStageFlagBit         stage;
        ShaderTarget               target     = ShaderTarget::SPIRV;
        ShaderFileSystem          *fileSystem = nullptr;
        const ShaderVariant       *variant    = nullptr;
        const ShaderVariantSchema *schema     = nullptr;
        ShaderCache               *cache      = nullptr;
        std::string                sourcePath;
        ShaderTargetEnvironment    environment;
        uint64_t                   sourceHash  = 0;
        uint64_t                   variantHash = 0;
        uint64_t                   layoutFp    = 0;
        uint64_t                   schemaFp    = 0;
    };

    struct ShaderCompileResult : ShaderStageArtifact {
        std::string errorInfo;
    };

} // namespace sky::aurora
