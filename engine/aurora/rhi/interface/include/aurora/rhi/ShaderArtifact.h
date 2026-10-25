//
// Created on 2026/10/23.
//

#pragma once

#include <aurora/rhi/ShaderReflection.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace sky::aurora {

    enum class ShaderTarget : uint32_t { SPIRV = 0, MSL, DXIL };
    enum class ShaderPlatform : uint32_t { UNKNOWN = 0, WINDOWS, MACOS, IOS, ANDROID_OS, LINUX };
    enum class ShaderCodeFormat : uint32_t { UNKNOWN = 0, SPIRV, MSL_SOURCE, DXIL, METALLIB };

    struct ShaderTargetEnvironment {
        ShaderTarget     target     = ShaderTarget::SPIRV;
        ShaderPlatform   platform   = ShaderPlatform::UNKNOWN;
        ShaderCodeFormat codeFormat = ShaderCodeFormat::UNKNOWN;
        std::string      profile;
        // Target language version: major * 10000 + minor * 100 + patch; zero selects the adapter baseline.
        uint32_t                 minimumVersion       = 0;
        uint64_t                 requiredCapabilities = 0;
        ShaderMatrixLayout       matrixLayout         = ShaderMatrixLayout::COLUMN_MAJOR;
        std::vector<std::string> options;
        bool                     operator==(const ShaderTargetEnvironment &) const = default;
    };

    struct ShaderProducerIdentity {
        std::string compiler;
        std::string version;
        uint32_t    adapterVersion                                   = 1;
        uint32_t    reflectionVersion                                = ShaderReflection::kProtocolVersion;
        uint64_t    configuration                                    = 0;
        bool        operator==(const ShaderProducerIdentity &) const = default;
    };

    enum class ShaderVariantType : uint8_t { UINT32, INT32, BOOL };

    struct ShaderVariantValue {
        std::string name;
        uint32_t    value                                        = 0;
        ShaderVariantType type = ShaderVariantType::UINT32;
        bool        operator==(const ShaderVariantValue &) const = default;
    };

    enum class ShaderVariantLowering : uint8_t { COMPILE, NATIVE };
    struct ShaderVariantBinding {
        std::string key;
        std::string symbol;
        ShaderVariantType type = ShaderVariantType::UINT32;
        ShaderVariantLowering lowering = ShaderVariantLowering::COMPILE;
        uint32_t nativeId = 0;
        uint32_t defaultValue = 0;
        bool operator==(const ShaderVariantBinding &) const = default;
    };

    struct ShaderStageIdentity {
        std::string                     sourcePath;
        std::string                     sourceEntry;
        std::string                     nativeEntry;
        ShaderStageFlagBit              stage       = ShaderStageFlagBit::VS;
        uint64_t                        sourceHash  = 0;
        uint64_t                        variantHash = 0;
        uint64_t                        layoutFp    = 0;
        uint64_t                        schemaFp    = 0;
        uint64_t                        interfaceFp = 0;
        std::vector<ShaderVariantValue> variants;
        ShaderTargetEnvironment         environment;
        ShaderProducerIdentity          producer;
        bool                            operator==(const ShaderStageIdentity &) const = default;
    };

    struct ShaderStageArtifact {
        static constexpr uint32_t kRuntimeAbiVersion = 1;
        uint32_t                  runtimeAbiVersion  = kRuntimeAbiVersion;
        ShaderStageIdentity       identity;
        std::vector<uint8_t>      data;
        ShaderReflection          reflection;
        std::vector<ShaderVariantBinding> variantBindings;
        uint64_t                  artifactId = 0;
    };
    using ShaderStageArtifactPtr = std::shared_ptr<const ShaderStageArtifact>;

    ShaderPlatform          CurrentShaderPlatform();
    ShaderTargetEnvironment DefaultShaderEnvironment(ShaderTarget target, ShaderPlatform platform = ShaderPlatform::UNKNOWN);
    bool                    NormalizeShaderEnvironment(ShaderTargetEnvironment &environment, std::string &error);
    uint64_t                ShaderEnvironmentFingerprint(const ShaderTargetEnvironment &environment);
    uint64_t                ShaderProducerFingerprint(const ShaderProducerIdentity &producer);
    uint64_t                ShaderInterfaceFingerprint(const ShaderReflection &reflection);
    uint64_t                ShaderReflectionFingerprint(const ShaderReflection &reflection);
    uint64_t                ComputeShaderArtifactId(const ShaderStageArtifact &artifact);
    bool                    ValidateShaderReflection(const ShaderReflection &reflection, std::string &error);
    bool                    ValidateShaderArtifact(const ShaderStageArtifact &artifact, std::string &error);
    bool                    SealShaderArtifact(ShaderStageArtifact &artifact, std::string &error);
    bool                    MergeShaderReflections(const ShaderReflection &stage, ShaderReflection &program, std::string &error);

} // namespace sky::aurora
