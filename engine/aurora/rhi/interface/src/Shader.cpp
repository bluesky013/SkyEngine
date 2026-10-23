//
// Created on 2026/10/23.
//

#include <aurora/rhi/Shader.h>

#include <algorithm>
#include <aurora/rhi/Device.h>
#include <cstring>
#include <set>

namespace sky::aurora {
    bool Shader::PackUniformData(uint32_t                                                 set,
                                 uint32_t                                                 binding,
                                 std::span<const std::pair<std::string, ShaderDataValue>> values,
                                 std::vector<std::byte>                                  &bytes,
                                 std::string                                             &error) const
    {
        ShaderCpuSchema        cpu;
        std::vector<std::byte> source;
        if (!BuildShaderDataSchema(values, cpu, source, error))
            return false;
        const auto plan = GetUniformPlan(set, binding, cpu, error);
        return plan && EncodeShaderUniform(*plan, source, bytes, error);
    }
    bool Shader::InitParameterTable(const Descriptor &desc, std::string &error)
    {
        const auto &first      = desc.ps ? desc.vs->GetArtifact() : desc.cs->GetArtifact();
        uint64_t    generation = first->artifactId;
        if (desc.ps)
            generation ^= desc.ps->GetArtifact()->artifactId + 0x9e3779b97f4a7c15ull + (generation << 6) + (generation >> 2);
        generation ^= ShaderReflectionFingerprint(GetReflection());
        return BuildShaderParameterTable(GetReflection(), generation ? generation : 1, desc.parameterAliases, parameters, error);
    }

    std::shared_ptr<const UniformUploadPlan>
    Shader::GetUniformPlan(uint32_t set, uint32_t binding, const ShaderCpuSchema &cpu, std::string &error) const
    {
        std::lock_guard lock(uniformPlanMutex);
        const auto      key = std::tuple{set, binding, ShaderCpuSchemaFingerprint(cpu)};
        if (const auto found = uniformPlans.find(key); found != uniformPlans.end())
            return found->second;
        const auto &blocks = GetReflection().blocks;
        const auto  block  = std::find_if(blocks.begin(), blocks.end(), [&](const auto &item) { return item.set == set && item.binding == binding; });
        if (block == blocks.end()) {
            error = "Uniform block is absent from this shader version";
            return {};
        }
        UniformUploadPlan plan;
        if (!BuildUniformUploadPlan(cpu, *block, parameters.generation, plan, error))
            return {};
        auto result = std::make_shared<const UniformUploadPlan>(std::move(plan));
        uniformPlans.emplace(key, result);
        return result;
    }

    bool ShaderFunction::InitArtifact(const Descriptor &desc, API backend, std::string &error)
    {
        if (!desc.artifact || !ValidateShaderArtifact(*desc.artifact, error)) {
            if (error.empty())
                error = "Shader function requires a complete stage artifact";
            return false;
        }
        const auto &input    = *desc.artifact;
        const auto  baseline = DefaultShaderEnvironment(input.identity.environment.target, input.identity.environment.platform);
        if (input.identity.environment.minimumVersion != baseline.minimumVersion || input.identity.environment.profile != baseline.profile ||
            input.identity.environment.requiredCapabilities) {
            error = "Shader device requirements are unsupported by this runtime adapter";
            return false;
        }
        const auto target = backend == API::METAL ? ShaderTarget::MSL : backend == API::DX12 ? ShaderTarget::DXIL : ShaderTarget::SPIRV;
        if ((backend != API::METAL && backend != API::DX12 && backend != API::VULKAN) || input.identity.environment.target != target ||
            input.identity.environment.platform != CurrentShaderPlatform() || input.identity.stage != desc.stage ||
            (!desc.entry.empty() && desc.entry != input.identity.nativeEntry)) {
            error = "Shader artifact target, platform, stage or native entry does not match the device request";
            return false;
        }
        if (desc.data) {
            auto *binary = desc.data->GetBinaryData();
            if (!binary || binary->Size() != input.data.size() || std::memcmp(binary->Data(), input.data.data(), input.data.size()) != 0) {
                error = "Separate shader payload does not match its artifact";
                return false;
            }
        }
        artifact = std::make_shared<const ShaderStageArtifact>(input);
        return true;
    }

    bool BuildShaderProgramReflection(const Shader::Descriptor &desc, const Device &device, ShaderReflection &reflection, std::string &error)
    {
        const bool                     graphics    = desc.ps != nullptr;
        const ShaderFunction          *functions[] = {graphics ? desc.vs : desc.cs, graphics ? desc.ps : nullptr};
        ShaderReflection               merged;
        const ShaderTargetEnvironment *environment = nullptr;
        for (size_t i = 0; i < (graphics ? 2u : 1u); ++i) {
            const auto *function = functions[i];
            const auto  stage    = graphics ? (i == 0 ? ShaderStageFlagBit::VS : ShaderStageFlagBit::FS) : ShaderStageFlagBit::CS;
            if (!function || function->GetDevice() != &device || !function->GetArtifact() || function->GetArtifact()->identity.stage != stage) {
                error = "Shader functions require matching stages, artifacts and device ownership";
                return false;
            }
            const auto &artifact = *function->GetArtifact();
            if (!ValidateShaderArtifact(artifact, error))
                return false;
            if (environment && *environment != artifact.identity.environment) {
                error = "Program stages have incompatible target environments";
                return false;
            }
            environment = &artifact.identity.environment;
            if (!MergeShaderReflections(artifact.reflection, merged, error))
                return false;
        }
        std::set<std::pair<uint32_t, uint32_t>> policies;
        for (const auto &policy : desc.bindingPolicies) {
            auto found = std::find_if(merged.resources.begin(), merged.resources.end(),
                                      [&](const auto &resource) { return resource.set == policy.set && resource.binding == policy.binding; });
            if (!policies.emplace(policy.set, policy.binding).second || found == merged.resources.end() ||
                !((found->type == ShaderResourceType::UNIFORM_BUFFER && policy.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC) ||
                  (found->type == ShaderResourceType::STORAGE_BUFFER && policy.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC))) {
                error = "Invalid or duplicate dynamic binding policy";
                return false;
            }
            found->type = policy.type;
        }
        if (desc.reflection && ShaderReflectionFingerprint(*desc.reflection) != ShaderReflectionFingerprint(merged)) {
            error = "Caller reflection does not match the linked stage artifacts";
            return false;
        }
        if (!ValidateShaderReflection(merged, error))
            return false;
        reflection = std::move(merged);
        return true;
    }
} // namespace sky::aurora
