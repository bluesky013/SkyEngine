//
// Aurora shader reflection types (slang program layout converges here).
// Backend-agnostic: produced by the shader compiler, consumed by RHI backends
// to build native pipeline layouts / root signatures / argument buffers.
//

#pragma once

#include <aurora/rhi/Core.h>

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

    // scalar type of a reflected field, used to reconstruct the C++ mirror
    enum class ShaderScalarType : uint32_t {
        UNKNOWN = 0,
        FLOAT,
        INT,
        UINT,
        BOOL,
    };

    // structural kind of a reflected field
    enum class ShaderTypeKind : uint32_t {
        UNKNOWN = 0,
        SCALAR,
        VECTOR,
        MATRIX,
        ARRAY,
        STRUCT,
    };

    // uniform block member (UBO field reflection)
    struct ShaderBlockMember {
        std::string      name;
        uint32_t         offset     = 0;
        uint32_t         size       = 0;
        ShaderScalarType scalarType = ShaderScalarType::UNKNOWN;
        ShaderTypeKind   kind       = ShaderTypeKind::UNKNOWN;
        uint32_t         rows       = 1;
        uint32_t         cols       = 1;
    };

    // uniform block (cbuffer / ParameterBlock) layout
    struct ShaderBlockLayout {
        std::string                    name;       // ParameterBlock variable name
        std::string                    structName; // element struct type name
        uint32_t                       set     = 0;
        uint32_t                       binding = 0;
        uint32_t                       size    = 0;
        std::vector<ShaderBlockMember> members;
    };

    struct ShaderReflection {
        std::vector<ShaderResource>    resources;
        std::vector<ShaderBlockLayout> blocks;
        std::vector<PushConstantRange> pushConstants;
    };

} // namespace sky::aurora
