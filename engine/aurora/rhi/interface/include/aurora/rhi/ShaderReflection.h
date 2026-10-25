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
        SAMPLER                = 0,
        SAMPLED_IMAGE          = 1,
        STORAGE_IMAGE          = 2,
        UNIFORM_BUFFER         = 3,
        STORAGE_BUFFER         = 4,
        INPUT_ATTACHMENT       = 5,
        UNIFORM_BUFFER_DYNAMIC = 6,
        STORAGE_BUFFER_DYNAMIC = 7,
    };

    enum class ShaderScalarType : uint32_t { UNKNOWN = 0, FLOAT, INT, UINT, BOOL };
    enum class ShaderTextureDimension : uint32_t { UNKNOWN = 0, D1, D2, D3, CUBE, BUFFER };
    enum class ShaderSamplerType : uint32_t { UNKNOWN = 0, REGULAR, COMPARISON };
    enum class ShaderResourceAccess : uint32_t { UNKNOWN = 0, READ_ONLY, READ_WRITE };
    enum class ShaderBufferKind : uint32_t { NONE = 0, RAW, STRUCTURED };
    enum class ShaderMatrixLayout : uint32_t { COLUMN_MAJOR = 0, ROW_MAJOR };
    enum class ShaderReflectionKind : uint32_t { DECLARATION = 0, STAGE, PROGRAM };
    enum class ShaderBindingModel : uint32_t { UNKNOWN = 0, VULKAN_DESCRIPTORS, D3D12_REGISTERS, METAL_DIRECT_ARGUMENTS };
    enum class ShaderUsageAnalysis : uint32_t { EXACT = 0, CONSERVATIVE };
    enum class ShaderNativeCategory : uint32_t { UNKNOWN = 0, DESCRIPTOR, CBV, SRV, UAV, SAMPLER, METAL_BUFFER, METAL_TEXTURE, METAL_SAMPLER };

    template <ShaderResourceType Type>
    struct ShaderResourceSlot {
        uint32_t set = 0, binding = 0;
    };

    struct ShaderTextureType {
        ShaderTextureDimension dimension  = ShaderTextureDimension::UNKNOWN;
        ShaderScalarType       scalarType = ShaderScalarType::UNKNOWN;
        uint32_t               components = 0;
        bool                   arrayed = false, multisampled = false;
        bool                   operator==(const ShaderTextureType &) const = default;
    };
    struct ShaderResource {
        std::string        name;
        ShaderResourceType type = ShaderResourceType::SAMPLER;
        uint32_t           set = 0, binding = 0, count = 1;
        ShaderTextureType  texture;
        ShaderSamplerType  sampler = ShaderSamplerType::UNKNOWN;
        // A descriptor array of length one is still an array; zero count means unbounded/unknown.
        bool                 descriptorArray = false;
        ShaderResourceAccess access          = ShaderResourceAccess::READ_ONLY;
        ShaderBufferKind     bufferKind      = ShaderBufferKind::NONE;
        uint32_t             elementStride   = 0;
        ShaderStageFlags     stages;
        std::string          declarationOwner;
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
        std::string        name;
        uint32_t           offset                                      = 0;
        uint32_t           size                                        = 0;
        ShaderScalarType   scalarType                                  = ShaderScalarType::UNKNOWN;
        ShaderTypeKind     kind                                        = ShaderTypeKind::UNKNOWN;
        uint32_t           rows                                        = 1;
        uint32_t           cols                                        = 1;
        uint32_t           arrayCount                                  = 0;
        uint32_t           arrayStride                                 = 0;
        bool               operator==(const ShaderBlockMember &) const = default;
        uint32_t           alignment                                   = 1;
        uint32_t           scalarBytes                                 = 4;
        uint32_t           matrixStride                                = 0;
        ShaderMatrixLayout matrixLayout                                = ShaderMatrixLayout::COLUMN_MAJOR;
    };

    // uniform block (cbuffer / ParameterBlock) layout
    struct ShaderBlockLayout {
        std::string                    name;       // ParameterBlock variable name
        std::string                    structName; // element struct type name
        uint32_t                       set     = 0;
        uint32_t                       binding = 0;
        uint32_t                       size    = 0;
        std::vector<ShaderBlockMember> members;
        uint32_t                       alignment = 1;
    };

    // Logical descriptor keys stay stable while native argument indices may differ between stages.
    struct ShaderNativeBinding {
        uint32_t             set = 0, binding = 0;
        ShaderStageFlagBit   stage                                         = ShaderStageFlagBit::VS;
        uint32_t             index                                         = 0;
        bool                 operator==(const ShaderNativeBinding &) const = default;
        ShaderNativeCategory category                                      = ShaderNativeCategory::UNKNOWN;
        uint32_t             space                                         = 0;
        uint32_t             count                                         = 1;
    };

    struct ShaderVertexInput {
        std::string semantic;
        uint32_t semanticIndex = 0, location = 0;
        ShaderScalarType scalarType = ShaderScalarType::UNKNOWN;
        uint32_t components = 0;
        bool operator==(const ShaderVertexInput &) const = default;
    };

    struct ShaderReflection {
        std::vector<ShaderVertexInput>   vertexInputs;
        std::vector<ShaderResource>      resources;
        std::vector<ShaderNativeBinding> nativeBindings;
        std::vector<ShaderBlockLayout>   blocks;
        std::vector<PushConstantRange>   pushConstants;
        // compute thread group size ([numthreads]); {0,0,0} when unknown or
        // non-compute. Targets that embed it in the binary (SPIRV/DXIL) may
        // ignore this; MSL does not carry it, so Metal needs it here.
        uint32_t                       threadGroupSize[3] = {0, 0, 0};
        static constexpr uint32_t      kProtocolVersion   = 1;
        uint32_t                       protocolVersion    = kProtocolVersion;
        ShaderReflectionKind           kind               = ShaderReflectionKind::DECLARATION;
        ShaderBindingModel             bindingModel       = ShaderBindingModel::UNKNOWN;
        ShaderUsageAnalysis            usage              = ShaderUsageAnalysis::EXACT;
        ShaderStageFlags               stages;
        bool                           complete = false;
        std::vector<ShaderResource>    declarations;
        std::vector<ShaderBlockLayout> declaredBlocks;
    };

} // namespace sky::aurora
