#pragma once

#include <aurora/rhi/ShaderArtifact.h>
#include <core/hash/Fnv1a.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <span>
#include <string>

namespace sky::aurora::test {

    // Callers provide the fixture's actual native payload and explicit address map.
    inline ShaderStageArtifactPtr MakeTestShaderArtifact(ShaderTarget             target,
                                                         ShaderStageFlagBit       stage,
                                                         std::span<const uint8_t> code,
                                                         const std::string       &nativeEntry,
                                                         ShaderReflection         reflection = {},
                                                         const std::string       &sourcePath = "rhi-test/native-fixture")
    {
        for (const auto &resource : reflection.resources) {
            if (std::none_of(reflection.declarations.begin(), reflection.declarations.end(),
                             [&](const auto &declaration) { return declaration.set == resource.set && declaration.binding == resource.binding; })) {
                ADD_FAILURE() << "Native fixture is missing its full logical declaration";
                return {};
            }
        }
        ShaderStageArtifact artifact;
        artifact.data.assign(code.begin(), code.end());
        auto &identity          = artifact.identity;
        identity.sourcePath     = sourcePath;
        identity.sourceEntry    = nativeEntry;
        identity.nativeEntry    = nativeEntry;
        identity.stage          = stage;
        identity.sourceHash     = sky::Fnv1a64(std::string_view(reinterpret_cast<const char *>(code.data()), code.size()));
        identity.variantHash    = sky::Fnv1a64("");
        identity.environment    = DefaultShaderEnvironment(target, CurrentShaderPlatform());
        identity.producer       = {"hand-authored-native-fixture", "1", 1, ShaderReflection::kProtocolVersion, 0};
        reflection.kind         = ShaderReflectionKind::STAGE;
        reflection.stages       = stage;
        reflection.complete     = true;
        reflection.bindingModel = target == ShaderTarget::SPIRV ? ShaderBindingModel::VULKAN_DESCRIPTORS
                                  : target == ShaderTarget::MSL ? ShaderBindingModel::METAL_DIRECT_ARGUMENTS
                                                                : ShaderBindingModel::D3D12_REGISTERS;
        artifact.reflection     = std::move(reflection);
        std::string error;
        if (!SealShaderArtifact(artifact, error)) {
            ADD_FAILURE() << error;
            return {};
        }
        return std::make_shared<const ShaderStageArtifact>(std::move(artifact));
    }

} // namespace sky::aurora::test
