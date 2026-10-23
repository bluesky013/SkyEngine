#pragma once

#include <aurora/rhi/ShaderArtifact.h>

#include <map>
#include <string>
#include <vector>

namespace sky::aurora {
    struct VulkanPlannedBinding {
        uint32_t           logicalSet      = 0;
        uint32_t           logicalBinding  = 0;
        uint32_t           physicalBinding = 0;
        uint32_t           count           = 0;
        uint32_t           slotBase        = 0;
        uint32_t           dynamicBase     = 0;
        ShaderResourceType type            = ShaderResourceType::SAMPLER;
        ShaderStageFlags   stages;
    };
    struct VulkanPlannedSet {
        uint32_t                          logicalSet = 0;
        std::vector<VulkanPlannedBinding> bindings;
        std::vector<uint32_t>             dynamicOffsets;
    };
    struct VulkanBindingPlan {
        std::map<uint32_t, VulkanPlannedSet>                  sets;
        std::map<uint32_t, std::vector<VulkanPlannedBinding>> logicalBindings;
        std::map<uint32_t, uint32_t>                          logicalSlotCounts;
        std::map<uint32_t, uint32_t>                          logicalDynamicCounts;
    };

    inline bool BuildVulkanBindingPlan(const ShaderReflection &reflection, uint32_t maximumSets, VulkanBindingPlan &out, std::string &error)
    {
        if (reflection.bindingModel != ShaderBindingModel::VULKAN_DESCRIPTORS || !ValidateShaderReflection(reflection, error)) {
            if (error.empty()) {
                error = "Vulkan requires complete descriptor target reflection";
            }
            return false;
        }
        VulkanBindingPlan                                             plan;
        std::map<std::pair<uint32_t, uint32_t>, VulkanPlannedBinding> logical;
        for (const auto &resource : reflection.resources) {
            VulkanPlannedBinding binding;
            binding.logicalSet     = resource.set;
            binding.logicalBinding = resource.binding;
            binding.type           = resource.type;
            binding.count          = resource.count;
            logical.emplace(std::pair{resource.set, resource.binding}, binding);
        }
        for (auto &[key, binding] : logical) {
            auto &slots = plan.logicalSlotCounts[key.first];
            if (binding.count > UINT32_MAX - slots) {
                error = "Vulkan descriptor slot count overflow";
                return false;
            }
            binding.slotBase = slots;
            slots += binding.count;
            if (binding.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || binding.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                auto &dynamic = plan.logicalDynamicCounts[key.first];
                if (binding.count > UINT32_MAX - dynamic) {
                    error = "Vulkan dynamic offset count overflow";
                    return false;
                }
                binding.dynamicBase = dynamic;
                dynamic += binding.count;
            }
            plan.logicalBindings[key.first].push_back(binding);
        }
        std::map<uint32_t, std::map<uint32_t, VulkanPlannedBinding>> physical;
        for (const auto &native : reflection.nativeBindings) {
            const auto found = logical.find({native.set, native.binding});
            if (found == logical.end() || native.category != ShaderNativeCategory::DESCRIPTOR || native.space >= maximumSets ||
                native.count != found->second.count) {
                error = "Invalid Vulkan physical resource address/category/count";
                return false;
            }
            auto binding            = found->second;
            binding.physicalBinding = native.index;
            binding.stages          = ShaderStageFlags(native.stage);
            auto &set               = physical[native.space];
            for (const auto &[index, existing] : set) {
                if (existing.logicalSet != binding.logicalSet) {
                    error = "Unsupported: multiple logical resource groups share one Vulkan physical set";
                    return false;
                }
            }
            const auto [position, inserted] = set.emplace(native.index, binding);
            if (!inserted) {
                auto &existing = position->second;
                if (existing.logicalSet != binding.logicalSet || existing.logicalBinding != binding.logicalBinding || existing.type != binding.type ||
                    existing.count != binding.count) {
                    error = "Conflicting Vulkan physical descriptor address";
                    return false;
                }
                existing.stages |= binding.stages;
            }
        }
        for (auto &[set, bindings] : physical) {
            auto &destination = plan.sets[set];
            for (const auto &[index, binding] : bindings) {
                destination.logicalSet = binding.logicalSet;
                destination.bindings.push_back(binding);
                if (binding.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || binding.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                    for (uint32_t element = 0; element < binding.count; ++element) {
                        destination.dynamicOffsets.push_back(binding.dynamicBase + element);
                    }
                }
            }
        }
        out = std::move(plan);
        return true;
    }
} // namespace sky::aurora
