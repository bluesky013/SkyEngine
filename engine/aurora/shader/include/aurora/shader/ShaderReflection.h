//
// Aurora shader reflection types (slang program layout converges here).
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sky::aurora {

    enum class ShaderResourceType : uint32_t {
        SAMPLER          = 0,
        SAMPLED_IMAGE    = 1,
        STORAGE_IMAGE    = 2,
        UNIFORM_BUFFER   = 3,
        STORAGE_BUFFER   = 4,
        INPUT_ATTACHMENT = 5,
    };

    struct ShaderResource {
        std::string        name;
        ShaderResourceType type = ShaderResourceType::SAMPLER;
        uint32_t           set     = 0;
        uint32_t           binding = 0;
        uint32_t           count   = 1;
    };

    // uniform block member (UBO field reflection)
    struct ShaderBlockMember {
        std::string name;
        uint32_t    offset = 0;
        uint32_t    size   = 0;
    };

    // uniform block (cbuffer / ParameterBlock) layout
    struct ShaderBlockLayout {
        std::string                    name;
        uint32_t                       set     = 0;
        uint32_t                       binding = 0;
        uint32_t                       size    = 0;
        std::vector<ShaderBlockMember> members;
    };

    struct ShaderReflection {
        std::vector<ShaderResource>    resources;
        std::vector<ShaderBlockLayout> blocks;
    };

} // namespace sky::aurora
