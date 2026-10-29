//
// Created on 2026/10/23.
//

#include <aurora/rhi/ShaderArtifact.h>
#include <core/platform/Platform.h>

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace sky::aurora {
    namespace {
        struct Digest {
            uint64_t value = 1469598103934665603ull;
            void     U64(uint64_t input)
            {
                for (uint32_t i = 0; i != 8; ++i) {
                    value = (value ^ ((input >> (8 * i)) & 255u)) * 1099511628211ull;
                }
            }
            void Bytes(std::span<const uint8_t> bytes)
            {
                U64(bytes.size());
                for (const auto byte : bytes)
                    value = (value ^ byte) * 1099511628211ull;
            }
            void Text(const std::string &text)
            {
                Bytes({reinterpret_cast<const uint8_t *>(text.data()), text.size()});
            }
        };

        bool SingleStage(ShaderStageFlagBit stage)
        {
            return stage == ShaderStageFlagBit::VS || stage == ShaderStageFlagBit::FS || stage == ShaderStageFlagBit::CS ||
                   stage == ShaderStageFlagBit::TAS || stage == ShaderStageFlagBit::MS;
        }
        bool PowerOfTwo(uint32_t value)
        {
            return value && (value & (value - 1)) == 0;
        }
        bool Fail(std::string &error, const char *message)
        {
            error = message;
            return false;
        }
        void HashResource(Digest &hash, const ShaderResource &resource, bool physical)
        {
            hash.Text(resource.declarationOwner);
            hash.Text(resource.name);
            hash.U64(static_cast<uint32_t>(resource.type));
            hash.U64(resource.set);
            hash.U64(resource.binding);
            hash.U64(resource.count);
            hash.U64(resource.descriptorArray);
            hash.U64(static_cast<uint32_t>(resource.texture.dimension));
            hash.U64(static_cast<uint32_t>(resource.texture.scalarType));
            hash.U64(resource.texture.components);
            hash.U64(resource.texture.arrayed);
            hash.U64(resource.texture.multisampled);
            hash.U64(static_cast<uint32_t>(resource.sampler));
            hash.U64(static_cast<uint32_t>(resource.access));
            hash.U64(static_cast<uint32_t>(resource.bufferKind));
            if (physical) {
                hash.U64(resource.elementStride);
                hash.U64(resource.stages.value);
            }
        }
        void HashBlock(Digest &hash, const ShaderBlockLayout &block, bool physical)
        {
            hash.Text(block.name);
            hash.Text(block.structName);
            hash.U64(block.set);
            hash.U64(block.binding);
            if (physical) {
                hash.U64(block.size);
                hash.U64(block.alignment);
            }
            hash.U64(block.members.size());
            for (const auto &member : block.members) {
                hash.Text(member.name);
                hash.U64(static_cast<uint32_t>(member.scalarType));
                hash.U64(static_cast<uint32_t>(member.kind));
                hash.U64(member.rows);
                hash.U64(member.cols);
                hash.U64(member.arrayCount);
                if (physical) {
                    hash.U64(member.offset);
                    hash.U64(member.size);
                    hash.U64(member.alignment);
                    hash.U64(member.scalarBytes);
                    hash.U64(member.arrayStride);
                    hash.U64(member.matrixStride);
                    hash.U64(static_cast<uint32_t>(member.matrixLayout));
                }
            }
        }
        void HashTables(Digest &hash, const std::vector<ShaderResource> &resources, const std::vector<ShaderBlockLayout> &blocks, bool physical)
        {
            std::vector<const ShaderResource *> orderedResources;
            for (const auto &resource : resources)
                orderedResources.push_back(&resource);
            std::ranges::sort(orderedResources, [](const auto *a, const auto *b) {
                return std::tie(a->set, a->binding, a->name) < std::tie(b->set, b->binding, b->name);
            });
            hash.U64(orderedResources.size());
            for (const auto *resource : orderedResources)
                HashResource(hash, *resource, physical);
            std::vector<const ShaderBlockLayout *> orderedBlocks;
            for (const auto &block : blocks)
                orderedBlocks.push_back(&block);
            std::ranges::sort(orderedBlocks,
                              [](const auto *a, const auto *b) { return std::tie(a->set, a->binding) < std::tie(b->set, b->binding); });
            hash.U64(orderedBlocks.size());
            for (const auto *block : orderedBlocks)
                HashBlock(hash, *block, physical);
        }
        bool SameResource(const ShaderResource &a, const ShaderResource &b)
        {
            return a.name == b.name && a.declarationOwner == b.declarationOwner && a.type == b.type && a.count == b.count &&
                   a.descriptorArray == b.descriptorArray && a.texture == b.texture && a.sampler == b.sampler && a.access == b.access &&
                   a.bufferKind == b.bufferKind && a.elementStride == b.elementStride;
        }
        bool BlocksValid(const std::vector<ShaderBlockLayout> &blocks, std::string &error)
        {
            std::set<std::pair<uint32_t, uint32_t>> keys;
            for (const auto &block : blocks) {
                if (block.name.empty() || !block.size || block.size > 65536 || !PowerOfTwo(block.alignment) ||
                    !keys.emplace(block.set, block.binding).second)
                    return Fail(error, "Invalid or duplicate uniform block");
                std::set<std::string>                      names;
                std::vector<std::pair<uint32_t, uint32_t>> ranges;
                for (const auto &member : block.members) {
                    if (member.name.empty() || !names.insert(member.name).second || !member.size || !PowerOfTwo(member.alignment) ||
                        member.offset > block.size || member.size > block.size - member.offset || member.scalarBytes != 4 ||
                        member.scalarType == ShaderScalarType::UNKNOWN || member.kind == ShaderTypeKind::UNKNOWN ||
                        member.kind == ShaderTypeKind::STRUCT || !member.rows || !member.cols || member.rows > 4 || member.cols > 4 ||
                        static_cast<uint32_t>(member.matrixLayout) > static_cast<uint32_t>(ShaderMatrixLayout::ROW_MAJOR))
                        return Fail(error, "Invalid uniform field layout");
                    if (member.arrayCount && (!member.arrayStride || member.arrayCount > member.size / member.arrayStride))
                        return Fail(error, "Invalid uniform array stride or count");
                    if (member.kind == ShaderTypeKind::MATRIX && (!member.matrixStride || member.matrixStride < 4 * member.scalarBytes))
                        return Fail(error, "Missing matrix storage stride");
                    for (const auto &range : ranges)
                        if (member.offset < range.second && range.first < member.offset + member.size)
                            return Fail(error, "Overlapping uniform fields");
                    ranges.emplace_back(member.offset, member.offset + member.size);
                }
            }
            return true;
        }
    } // namespace

    ShaderPlatform CurrentShaderPlatform()
    {
#if defined(SKY_PLATFORM_WINDOWS)
        return ShaderPlatform::WINDOWS;
#elif defined(SKY_PLATFORM_IOS)
        return ShaderPlatform::IOS;
#elif defined(SKY_PLATFORM_MACOS)
        return ShaderPlatform::MACOS;
#elif defined(SKY_PLATFORM_ANDROID)
        return ShaderPlatform::ANDROID_OS;
#elif defined(SKY_PLATFORM_LINUX)
        return ShaderPlatform::LINUX;
#else
        return ShaderPlatform::UNKNOWN;
#endif
    }

    ShaderTargetEnvironment DefaultShaderEnvironment(ShaderTarget target, ShaderPlatform platform)
    {
        ShaderTargetEnvironment result;
        result.target         = target;
        result.platform       = platform == ShaderPlatform::UNKNOWN ? CurrentShaderPlatform() : platform;
        result.codeFormat     = target == ShaderTarget::SPIRV ? ShaderCodeFormat::SPIRV
                                : target == ShaderTarget::MSL ? ShaderCodeFormat::MSL_SOURCE
                                                              : ShaderCodeFormat::DXIL;
        if (target == ShaderTarget::SPIRV) {
            const bool androidTarget = result.platform == ShaderPlatform::ANDROID_OS;
            result.profile           = androidTarget ? "spirv_1_3" : "spirv_1_5";
            result.minimumVersion    = androidTarget ? 10300 : 10500;
        } else {
            result.profile        = target == ShaderTarget::MSL ? "metallib_2_3" : "sm_6_5";
            result.minimumVersion = target == ShaderTarget::MSL ? 20300 : 60500;
        }
        return result;
    }

    bool NormalizeShaderEnvironment(ShaderTargetEnvironment &environment, std::string &error)
    {
        if (static_cast<uint32_t>(environment.target) > static_cast<uint32_t>(ShaderTarget::DXIL))
            return Fail(error, "Unsupported shader target");
        const auto defaults = DefaultShaderEnvironment(environment.target, environment.platform);
        if (environment.platform == ShaderPlatform::UNKNOWN)
            environment.platform = defaults.platform;
        if (environment.codeFormat == ShaderCodeFormat::UNKNOWN)
            environment.codeFormat = defaults.codeFormat;
        if (environment.profile.empty())
            environment.profile = defaults.profile;
        if (!environment.minimumVersion)
            environment.minimumVersion = defaults.minimumVersion;
        if (environment.platform == ShaderPlatform::UNKNOWN ||
            static_cast<uint32_t>(environment.platform) > static_cast<uint32_t>(ShaderPlatform::LINUX) ||
            static_cast<uint32_t>(environment.matrixLayout) > static_cast<uint32_t>(ShaderMatrixLayout::ROW_MAJOR))
            return Fail(error, "Invalid shader platform or matrix convention");
        if ((environment.target == ShaderTarget::SPIRV && environment.codeFormat != ShaderCodeFormat::SPIRV) ||
            (environment.target == ShaderTarget::DXIL && environment.codeFormat != ShaderCodeFormat::DXIL) ||
            (environment.target == ShaderTarget::MSL && environment.codeFormat != ShaderCodeFormat::MSL_SOURCE &&
             environment.codeFormat != ShaderCodeFormat::METALLIB))
            return Fail(error, "Shader target and code format disagree");
        std::ranges::sort(environment.options);
        environment.options.erase(std::unique(environment.options.begin(), environment.options.end()), environment.options.end());
        return true;
    }

    uint64_t ShaderEnvironmentFingerprint(const ShaderTargetEnvironment &environment)
    {
        Digest hash;
        hash.U64(static_cast<uint32_t>(environment.target));
        hash.U64(static_cast<uint32_t>(environment.platform));
        hash.U64(static_cast<uint32_t>(environment.codeFormat));
        hash.Text(environment.profile);
        hash.U64(environment.minimumVersion);
        hash.U64(environment.requiredCapabilities);
        hash.U64(static_cast<uint32_t>(environment.matrixLayout));
        auto options = environment.options;
        std::ranges::sort(options);
        hash.U64(options.size());
        for (const auto &option : options)
            hash.Text(option);
        return hash.value;
    }

    uint64_t ShaderProducerFingerprint(const ShaderProducerIdentity &producer)
    {
        Digest hash;
        hash.Text(producer.compiler);
        hash.Text(producer.version);
        hash.U64(producer.adapterVersion);
        hash.U64(producer.reflectionVersion);
        hash.U64(producer.configuration);
        return hash.value;
    }

    uint64_t ShaderInterfaceFingerprint(const ShaderReflection &reflection)
    {
        Digest hash;
        hash.U64(reflection.vertexInputs.size());
        for (const auto &input : reflection.vertexInputs) {
            hash.Text(input.semantic);
            hash.U64(input.semanticIndex);
            hash.U64(input.location);
            hash.U64(static_cast<uint32_t>(input.scalarType));
            hash.U64(input.components);
        }
        HashTables(hash, reflection.declarations.empty() ? reflection.resources : reflection.declarations,
                   reflection.declaredBlocks.empty() ? reflection.blocks : reflection.declaredBlocks, false);
        return hash.value;
    }

    uint64_t ShaderReflectionFingerprint(const ShaderReflection &reflection)
    {
        Digest hash;
        hash.U64(reflection.protocolVersion);
        hash.U64(static_cast<uint32_t>(reflection.kind));
        hash.U64(static_cast<uint32_t>(reflection.bindingModel));
        hash.U64(static_cast<uint32_t>(reflection.usage));
        hash.U64(reflection.stages.value);
        hash.U64(reflection.complete);
        hash.U64(reflection.vertexInputs.size());
        for (const auto &input : reflection.vertexInputs) {
            hash.Text(input.semantic);
            hash.U64(input.semanticIndex);
            hash.U64(input.location);
            hash.U64(static_cast<uint32_t>(input.scalarType));
            hash.U64(input.components);
        }
        HashTables(hash, reflection.resources, reflection.blocks, true);
        HashTables(hash, reflection.declarations, reflection.declaredBlocks, true);
        auto bindings = reflection.nativeBindings;
        std::ranges::sort(bindings, [](const auto &a, const auto &b) {
            return std::tie(a.set, a.binding, a.stage, a.category, a.space, a.index) <
                   std::tie(b.set, b.binding, b.stage, b.category, b.space, b.index);
        });
        hash.U64(bindings.size());
        for (const auto &binding : bindings) {
            hash.U64(binding.set);
            hash.U64(binding.binding);
            hash.U64(static_cast<uint32_t>(binding.stage));
            hash.U64(binding.index);
            hash.U64(static_cast<uint32_t>(binding.category));
            hash.U64(binding.space);
            hash.U64(binding.count);
        }
        hash.U64(reflection.pushConstants.size());
        for (const auto &range : reflection.pushConstants) {
            hash.U64(range.stageFlags.value);
            hash.U64(range.offset);
            hash.U64(range.size);
        }
        for (const auto value : reflection.threadGroupSize)
            hash.U64(value);
        return hash.value;
    }

    uint64_t ComputeShaderArtifactId(const ShaderStageArtifact &artifact)
    {
        const auto &identity = artifact.identity;
        Digest      hash;
        hash.U64(artifact.runtimeAbiVersion);
        hash.U64(ShaderEnvironmentFingerprint(identity.environment));
        hash.U64(ShaderProducerFingerprint(identity.producer));
        hash.Text(identity.sourcePath);
        hash.Text(identity.sourceEntry);
        hash.Text(identity.nativeEntry);
        hash.U64(static_cast<uint32_t>(identity.stage));
        hash.U64(identity.sourceHash);
        hash.U64(identity.variantHash);
        hash.U64(identity.layoutFp);
        hash.U64(identity.schemaFp);
        hash.U64(identity.interfaceFp);
        auto variants = identity.variants;
        std::ranges::sort(variants, {}, &ShaderVariantValue::name);
        hash.U64(variants.size());
        for (const auto &variant : variants) {
            hash.Text(variant.name);
            hash.U64(variant.value);
            hash.U64(static_cast<uint32_t>(variant.type));
        }
        hash.U64(artifact.variantBindings.size());
        for (const auto &binding : artifact.variantBindings) {
            hash.Text(binding.key);
            hash.Text(binding.symbol);
            hash.U64(static_cast<uint32_t>(binding.type));
            hash.U64(static_cast<uint32_t>(binding.lowering));
            hash.U64(binding.nativeId);
            hash.U64(binding.defaultValue);
        }
        hash.Bytes(artifact.data);
        hash.U64(ShaderReflectionFingerprint(artifact.reflection));
        return hash.value;
    }

    bool ValidateShaderReflection(const ShaderReflection &reflection, std::string &error)
    {
        constexpr uint32_t supportedStages = 0x187;
        if (reflection.protocolVersion != ShaderReflection::kProtocolVersion || !reflection.complete ||
            reflection.kind == ShaderReflectionKind::DECLARATION || !reflection.stages || (reflection.stages.value & ~supportedStages) ||
            reflection.resources.size() > 65536 || reflection.declarations.size() > 65536 || reflection.nativeBindings.size() > 65536 ||
            reflection.blocks.size() > 4096 || reflection.declaredBlocks.size() > 4096 || reflection.pushConstants.size() > 32 ||
            reflection.bindingModel == ShaderBindingModel::UNKNOWN ||
            static_cast<uint32_t>(reflection.bindingModel) > static_cast<uint32_t>(ShaderBindingModel::METAL_DIRECT_ARGUMENTS) ||
            static_cast<uint32_t>(reflection.usage) > static_cast<uint32_t>(ShaderUsageAnalysis::CONSERVATIVE))
            return Fail(error, "Missing or incompatible target reflection protocol");
        if (!BlocksValid(reflection.blocks, error) || !BlocksValid(reflection.declaredBlocks, error))
            return false;
        using Key = std::pair<uint32_t, uint32_t>;
        std::map<Key, const ShaderResource *> resources;
        for (const auto &resource : reflection.resources) {
            if (resource.name.empty() || !resource.count || !resource.stages || (resource.stages & reflection.stages) != resource.stages ||
                !resources.emplace(Key{resource.set, resource.binding}, &resource).second || resource.access == ShaderResourceAccess::UNKNOWN)
                return Fail(error, "Invalid or duplicate active shader resource");
            if (resource.type == ShaderResourceType::STORAGE_BUFFER || resource.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                if ((resource.bufferKind != ShaderBufferKind::RAW && resource.bufferKind != ShaderBufferKind::STRUCTURED) ||
                    (resource.bufferKind == ShaderBufferKind::RAW && resource.elementStride != 0) ||
                    (resource.bufferKind == ShaderBufferKind::STRUCTURED && (!resource.elementStride || resource.elementStride % 4)))
                    return Fail(error, "Unsupported storage buffer shape or stride");
            }
        }
        std::set<std::tuple<uint32_t, uint32_t, ShaderStageFlagBit>>                                                         logicalStages;
        std::map<std::tuple<ShaderStageFlagBit, ShaderNativeCategory, uint32_t>, std::vector<std::pair<uint32_t, uint32_t>>> slots;
        for (const auto &binding : reflection.nativeBindings) {
            const auto found = resources.find({binding.set, binding.binding});
            if (found == resources.end() || !SingleStage(binding.stage) || !found->second->stages.TestBit(binding.stage) ||
                binding.category == ShaderNativeCategory::UNKNOWN || binding.count != found->second->count ||
                !logicalStages.emplace(binding.set, binding.binding, binding.stage).second || binding.count > UINT32_MAX - binding.index)
                return Fail(error, "Orphaned, duplicate, or incompatible native binding");
            const auto &resource = *found->second;
            const bool  uniform  = resource.type == ShaderResourceType::UNIFORM_BUFFER || resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
            const bool  buffer =
                uniform || resource.type == ShaderResourceType::STORAGE_BUFFER || resource.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
            const auto metalCategory = buffer                                         ? ShaderNativeCategory::METAL_BUFFER
                                       : resource.type == ShaderResourceType::SAMPLER ? ShaderNativeCategory::METAL_SAMPLER
                                                                                      : ShaderNativeCategory::METAL_TEXTURE;
            const auto d3dCategory   = uniform                                        ? ShaderNativeCategory::CBV
                                       : resource.type == ShaderResourceType::SAMPLER ? ShaderNativeCategory::SAMPLER
                                       : resource.type == ShaderResourceType::STORAGE_IMAGE || resource.access == ShaderResourceAccess::READ_WRITE
                                           ? ShaderNativeCategory::UAV
                                           : ShaderNativeCategory::SRV;
            if ((reflection.bindingModel == ShaderBindingModel::VULKAN_DESCRIPTORS && binding.category != ShaderNativeCategory::DESCRIPTOR) ||
                (reflection.bindingModel == ShaderBindingModel::D3D12_REGISTERS && binding.category != d3dCategory) ||
                (reflection.bindingModel == ShaderBindingModel::METAL_DIRECT_ARGUMENTS && (binding.category != metalCategory || binding.space)))
                return Fail(error, "Native resource category is incompatible with its target semantic type");
            if (reflection.bindingModel == ShaderBindingModel::METAL_DIRECT_ARGUMENTS &&
                (found->second->descriptorArray || binding.count != 1 ||
                 (binding.category != ShaderNativeCategory::METAL_BUFFER && binding.category != ShaderNativeCategory::METAL_TEXTURE &&
                  binding.category != ShaderNativeCategory::METAL_SAMPLER)))
                return Fail(error, "Unsupported Metal binding model");
            const uint32_t addressCount = binding.category == ShaderNativeCategory::DESCRIPTOR ? 1 : binding.count;
            auto          &ranges       = slots[{binding.stage, binding.category, binding.space}];
            for (const auto &[first, end] : ranges)
                if (binding.index < end && first < binding.index + addressCount)
                    return Fail(error, "Native shader resource address collision");
            ranges.emplace_back(binding.index, binding.index + addressCount);
        }
        constexpr ShaderStageFlagBit stages[] = {ShaderStageFlagBit::VS, ShaderStageFlagBit::FS, ShaderStageFlagBit::CS, ShaderStageFlagBit::TAS,
                                                 ShaderStageFlagBit::MS};
        for (const auto &[key, resource] : resources) {
            for (const auto stage : stages)
                if (resource->stages.TestBit(stage) && !logicalStages.count({key.first, key.second, stage}))
                    return Fail(error, "Active shader resource has no native address");
            if ((resource->type == ShaderResourceType::UNIFORM_BUFFER || resource->type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC) &&
                std::ranges::none_of(reflection.blocks, [&](const auto &block) { return block.set == key.first && block.binding == key.second; }))
                return Fail(error, "Active uniform buffer has no target block layout");
        }
        std::set<Key>                              declarationKeys;
        std::set<std::pair<uint32_t, std::string>> names;
        for (const auto &declaration : reflection.declarations) {
            if (declaration.name.empty() || declaration.declarationOwner.empty() || !declaration.count ||
                !declarationKeys.emplace(declaration.set, declaration.binding).second || !names.emplace(declaration.set, declaration.name).second)
                return Fail(error, "Duplicate or ambiguous logical parameter declaration");
        }
        for (const auto &[key, resource] : resources) {
            const auto declaration =
                std::ranges::find_if(reflection.declarations, [&](const auto &item) { return item.set == key.first && item.binding == key.second; });
            if (declaration == reflection.declarations.end())
                return Fail(error, "Active resource is absent from its logical interface");
            const auto baseType = [](ShaderResourceType type) {
                return type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC   ? ShaderResourceType::UNIFORM_BUFFER
                       : type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC ? ShaderResourceType::STORAGE_BUFFER
                                                                            : type;
            };
            if (resource->name != declaration->name || resource->declarationOwner != declaration->declarationOwner ||
                baseType(resource->type) != baseType(declaration->type) || resource->count != declaration->count ||
                resource->descriptorArray != declaration->descriptorArray || resource->texture != declaration->texture ||
                resource->sampler != declaration->sampler || resource->access != declaration->access ||
                resource->bufferKind != declaration->bufferKind)
                return Fail(error, "Active resource differs from its logical declaration");
        }
        for (const auto &push : reflection.pushConstants)
            if (!push.size || push.offset > UINT32_MAX - push.size || !push.stageFlags || (push.stageFlags.value & ~reflection.stages.value))
                return Fail(error, "Invalid push constant range or stages");
        return true;
    }

    bool ValidateShaderArtifact(const ShaderStageArtifact &artifact, std::string &error)
    {
        const auto &identity    = artifact.identity;
        auto        environment = identity.environment;
        if (artifact.runtimeAbiVersion != ShaderStageArtifact::kRuntimeAbiVersion || artifact.data.empty() ||
            artifact.data.size() > 64u * 1024u * 1024u || identity.sourcePath.empty() || identity.sourcePath.front() == '/' ||
            identity.sourcePath.find("..") != std::string::npos || identity.sourceEntry.empty() || identity.nativeEntry.empty() ||
            !SingleStage(identity.stage) || !identity.sourceHash || identity.producer.compiler.empty() || identity.producer.version.empty() ||
            !identity.producer.adapterVersion || identity.producer.reflectionVersion != ShaderReflection::kProtocolVersion)
            return Fail(error, "Incomplete shader artifact identity");
        std::set<std::string> variantKeys;
        std::set<uint32_t> nativeIds;
        for (const auto &binding : artifact.variantBindings) {
            if (binding.key.empty() || !variantKeys.insert(binding.key).second || binding.type > ShaderVariantType::BOOL ||
                binding.lowering > ShaderVariantLowering::NATIVE || (binding.type == ShaderVariantType::BOOL && binding.defaultValue > 1) ||
                (binding.lowering == ShaderVariantLowering::NATIVE && (binding.symbol.empty() || environment.target == ShaderTarget::DXIL ||
                                                                     !nativeIds.insert(binding.nativeId).second)))
                return Fail(error, "Invalid target variant mapping");
        }
        for (const auto &value : identity.variants)
            if (value.name.empty() || value.type > ShaderVariantType::BOOL || (value.type == ShaderVariantType::BOOL && value.value > 1))
                return Fail(error, "Invalid typed shader variant");
        if (!NormalizeShaderEnvironment(environment, error) || environment != identity.environment)
            return Fail(error, "Shader environment is not canonical");
        if (artifact.reflection.kind != ShaderReflectionKind::STAGE || artifact.reflection.stages != ShaderStageFlags(identity.stage) ||
            !ValidateShaderReflection(artifact.reflection, error))
            return Fail(error, "Shader artifact reflection does not match its stage");
        const auto expectedModel = environment.target == ShaderTarget::SPIRV ? ShaderBindingModel::VULKAN_DESCRIPTORS
                                   : environment.target == ShaderTarget::MSL ? ShaderBindingModel::METAL_DIRECT_ARGUMENTS
                                                                             : ShaderBindingModel::D3D12_REGISTERS;
        if (artifact.reflection.bindingModel != expectedModel || identity.interfaceFp != ShaderInterfaceFingerprint(artifact.reflection) ||
            !artifact.artifactId || artifact.artifactId != ComputeShaderArtifactId(artifact))
            return Fail(error, "Shader artifact code, interface, or reflection identity mismatch");
        if (environment.codeFormat == ShaderCodeFormat::SPIRV && (artifact.data.size() < 20 || artifact.data.size() % 4 || artifact.data[0] != 3 ||
                                                                  artifact.data[1] != 2 || artifact.data[2] != 35 || artifact.data[3] != 7))
            return Fail(error, "Invalid SPIR-V artifact payload");
        return true;
    }

    bool SealShaderArtifact(ShaderStageArtifact &artifact, std::string &error)
    {
        if (!NormalizeShaderEnvironment(artifact.identity.environment, error))
            return false;
        std::ranges::sort(artifact.identity.variants, {}, &ShaderVariantValue::name);
        for (size_t i = 0; i < artifact.identity.variants.size(); ++i)
            if (artifact.identity.variants[i].name.empty() || (i && artifact.identity.variants[i - 1].name == artifact.identity.variants[i].name))
                return Fail(error, "Duplicate or empty shader variant name");
        artifact.identity.interfaceFp = ShaderInterfaceFingerprint(artifact.reflection);
        artifact.artifactId           = ComputeShaderArtifactId(artifact);
        return ValidateShaderArtifact(artifact, error);
    }

    bool MergeShaderReflections(const ShaderReflection &stage, ShaderReflection &program, std::string &error)
    {
        if (!ValidateShaderReflection(stage, error))
            return false;
        if (program.complete && program.bindingModel != stage.bindingModel)
            return Fail(error, "Shader stages use different native binding models");
        auto merged         = program;
        merged.kind         = ShaderReflectionKind::PROGRAM;
        merged.complete     = true;
        merged.bindingModel = stage.bindingModel;
        merged.stages |= stage.stages;
        if (!stage.vertexInputs.empty()) {
            if (!merged.vertexInputs.empty() && merged.vertexInputs != stage.vertexInputs)
                return Fail(error, "Shader stages disagree on vertex inputs");
            merged.vertexInputs = stage.vertexInputs;
        }
        if (stage.usage == ShaderUsageAnalysis::CONSERVATIVE)
            merged.usage = stage.usage;
        const auto mergeResources = [&](const auto &inputs, auto &outputs) {
            for (const auto &resource : inputs) {
                const auto found =
                    std::ranges::find_if(outputs, [&](const auto &item) { return item.set == resource.set && item.binding == resource.binding; });
                if (found == outputs.end())
                    outputs.push_back(resource);
                else if (!SameResource(*found, resource))
                    return false;
                else
                    found->stages |= resource.stages;
            }
            return true;
        };
        const auto mergeBlocks = [&](const auto &inputs, auto &outputs) {
            for (const auto &block : inputs) {
                const auto found =
                    std::ranges::find_if(outputs, [&](const auto &item) { return item.set == block.set && item.binding == block.binding; });
                if (found == outputs.end())
                    outputs.push_back(block);
                else if (found->size != block.size || found->alignment != block.alignment || found->members != block.members)
                    return false;
            }
            return true;
        };
        if (!mergeResources(stage.resources, merged.resources) || !mergeResources(stage.declarations, merged.declarations))
            return Fail(error, "Shader stages disagree on a logical resource");
        if (!mergeBlocks(stage.blocks, merged.blocks) || !mergeBlocks(stage.declaredBlocks, merged.declaredBlocks))
            return Fail(error, "Unsupported stage-specific uniform block ABI");
        for (const auto &binding : stage.nativeBindings) {
            const auto found = std::ranges::find_if(merged.nativeBindings, [&](const auto &item) {
                return item.set == binding.set && item.binding == binding.binding && item.stage == binding.stage;
            });
            if (found == merged.nativeBindings.end())
                merged.nativeBindings.push_back(binding);
            else if (*found != binding)
                return Fail(error, "Shader stages disagree on a native resource address");
        }
        for (const auto &range : stage.pushConstants)
            merged.pushConstants.push_back(range);
        if (stage.stages.TestBit(ShaderStageFlagBit::CS))
            std::ranges::copy(stage.threadGroupSize, merged.threadGroupSize);
        if (!ValidateShaderReflection(merged, error))
            return false;
        program = std::move(merged);
        return true;
    }
} // namespace sky::aurora
