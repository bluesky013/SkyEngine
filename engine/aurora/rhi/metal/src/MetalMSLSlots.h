//
// MSL argument slots shared by shader validation and resource binding.
//

#pragma once

#include "MetalLimits.h"
#include <MetalShader.h>
#include <aurora/rhi/ShaderArtifact.h>

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace sky::aurora {

    struct MetalMSLStageExtent {
        uint32_t bufferCount = 0, textureCount = 0, samplerCount = 0;

        uint32_t PushConstantSlot() const
        {
            return bufferCount > 0 ? bufferCount - 1 : 0;
        }
    };

    struct MetalMSLBindingPlan {
        std::unordered_map<uint32_t, MetalResourceSetPlan> sets;
        std::array<MetalMSLStageExtent, 3>                 stages;
    };

    inline bool IsMetalBufferResource(ShaderResourceType type)
    {
        return type == ShaderResourceType::UNIFORM_BUFFER || type == ShaderResourceType::STORAGE_BUFFER ||
               type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
    }

    inline bool IsMetalTextureResource(ShaderResourceType type)
    {
        return type == ShaderResourceType::SAMPLED_IMAGE || type == ShaderResourceType::STORAGE_IMAGE || type == ShaderResourceType::INPUT_ATTACHMENT;
    }

    inline bool BuildMetalMSLBindingPlan(const ShaderReflection &reflection, MetalMSLBindingPlan &out, std::string &error)
    {
        if (!ValidateShaderReflection(reflection, error)) {
            return false;
        }
        if (reflection.bindingModel != ShaderBindingModel::METAL_DIRECT_ARGUMENTS) {
            error = "Metal requires DirectArguments reflection";
            return false;
        }
        MetalMSLBindingPlan plan;
        for (const auto &declaration : reflection.declarations) {
            plan.sets.try_emplace(declaration.set);
        }
        const ShaderStageFlagBit                    stages[] = {ShaderStageFlagBit::VS, ShaderStageFlagBit::FS, ShaderStageFlagBit::CS};
        std::array<std::unordered_set<uint32_t>, 3> usedBuffers, usedTextures, usedSamplers;
        for (const auto &resource : reflection.resources) {
            const auto foundSet = plan.sets.find(resource.set);
            if (foundSet == plan.sets.end()) {
                error = "Metal active resource has no declared logical set";
                return false;
            }
            auto &setPlan = foundSet->second;
            if (resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || resource.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                setPlan.dynamicBindings.push_back(resource.binding);
            }
            for (uint32_t stageIndex = 0; stageIndex < 3; ++stageIndex) {
                if (!(resource.stages & stages[stageIndex]))
                    continue;
                auto &stageSlots  = setPlan.stages[stageIndex];
                auto &stageExtent = plan.stages[stageIndex];
                auto *slots       = IsMetalBufferResource(resource.type)           ? &stageSlots.buffer
                                    : IsMetalTextureResource(resource.type)        ? &stageSlots.texture
                                    : resource.type == ShaderResourceType::SAMPLER ? &stageSlots.sampler
                                                                                   : nullptr;
                auto *used        = IsMetalBufferResource(resource.type)    ? &usedBuffers[stageIndex]
                                    : IsMetalTextureResource(resource.type) ? &usedTextures[stageIndex]
                                                                            : &usedSamplers[stageIndex];
                auto *extent      = IsMetalBufferResource(resource.type)    ? &stageExtent.bufferCount
                                    : IsMetalTextureResource(resource.type) ? &stageExtent.textureCount
                                                                            : &stageExtent.samplerCount;
                if (slots == nullptr) {
                    error = "Unsupported Metal resource category";
                    return false;
                }
                const ShaderNativeBinding *native = nullptr;
                for (const auto &binding : reflection.nativeBindings) {
                    if (binding.set != resource.set || binding.binding != resource.binding || binding.stage != stages[stageIndex])
                        continue;
                    if (native) {
                        error = "Duplicate Metal native resource address";
                        return false;
                    }
                    native = &binding;
                }
                if (!native || resource.count != 1 || resource.descriptorArray) {
                    error = "Metal resource requires one explicit native address";
                    return false;
                }
                const uint32_t index = native->index;
                const uint32_t limit = IsMetalBufferResource(resource.type)    ? METAL_MAX_BUFFER_SLOTS
                                       : IsMetalTextureResource(resource.type) ? METAL_MAX_TEXTURE_SLOTS
                                                                               : METAL_MAX_SAMPLER_SLOTS;
                if (index >= limit || !used->insert(index).second || !slots->emplace(resource.binding, index).second) {
                    error = "Metal native resource slot is invalid or overlaps";
                    return false;
                }
                *extent = std::max(*extent, index + 1);
            }
        }
        for (auto &[set, setPlan] : plan.sets) {
            std::sort(setPlan.dynamicBindings.begin(), setPlan.dynamicBindings.end());
        }
        out = std::move(plan);
        return true;
    }

} // namespace sky::aurora
