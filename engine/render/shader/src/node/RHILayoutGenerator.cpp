//
// Created by blues on 2025/4/21.
//

#include <shader/node/RHILayoutGenerator.h>
#include <shader/node/ShaderNode.h>
#include <algorithm>
#include <numeric>

namespace sky::sl {

    static rhi::DescriptorType MapNodeToDescriptorType(const ResourceDecl &node)
    {
        switch (node.type) {
        case ResourceType::CONSTANT_BUFFER:
            return node.dynamic
                ? rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC
                : rhi::DescriptorType::UNIFORM_BUFFER;
        case ResourceType::STRUCTURED_BUFFER:
            return rhi::DescriptorType::STORAGE_BUFFER;
        case ResourceType::TEXTURE:
            return node.storage
                ? rhi::DescriptorType::STORAGE_IMAGE
                : rhi::DescriptorType::SAMPLED_IMAGE;
        case ResourceType::SAMPLER:
            return rhi::DescriptorType::SAMPLER;
        }
        return rhi::DescriptorType::SAMPLER;
    }

    struct BindingAllocator {
        uint32_t nextBinding = 0;

        uint32_t Allocate(const ResourceDecl &node)
        {
            uint32_t binding = (node.binding != UINT32_MAX) ? node.binding : nextBinding;
            nextBinding = std::max(nextBinding, binding + 1);
            return binding;
        }
    };

    static std::vector<const ResourceDecl *> SortResources(
        std::span<const ResourceDecl> resources)
    {
        std::vector<const ResourceDecl *> sorted;
        sorted.reserve(resources.size());
        for (const auto &r : resources) {
            sorted.push_back(&r);
        }
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto *a, const auto *b) {
            uint32_t ba = (a->binding != UINT32_MAX) ? a->binding : UINT32_MAX;
            uint32_t bb = (b->binding != UINT32_MAX) ? b->binding : UINT32_MAX;
            return ba < bb;
        });
        return sorted;
    }

    static void ProcessResources(
        std::span<const ResourceDecl> resources,
        const ResourceGroupDecl &group,
        BindingAllocator &alloc,
        rhi::DescriptorSetLayout::Descriptor &desc)
    {
        auto sorted = SortResources(resources);
        for (const auto *res : sorted) {
            uint32_t binding = alloc.Allocate(*res);

            auto visibility = res->visibility;
            if (visibility == rhi::ShaderStageFlags{}) {
                visibility = group.defaultVisibility;
            }

            rhi::DescriptorSetLayout::SetBinding sb = {};
            sb.type       = MapNodeToDescriptorType(*res);
            sb.count      = 1;
            sb.binding    = binding;
            sb.visibility = visibility;
            sb.name       = std::string(res->name);
            desc.bindings.emplace_back(sb);
        }
    }

    rhi::DescriptorSetLayout::Descriptor RHILayoutGenerator::Generate(
        const ResourceGroupDecl &group)
    {
        rhi::DescriptorSetLayout::Descriptor desc = {};
        BindingAllocator alloc;

        ProcessResources(group.resources, group, alloc, desc);

        for (const auto &cond : group.conditionals) {
            ProcessResources(cond.resources, group, alloc, desc);
        }

        return desc;
    }

    rhi::DescriptorSetLayout::Descriptor RHILayoutGenerator::GenerateWithConditions(
        const ResourceGroupDecl &group,
        const std::set<std::string> &activeConditions)
    {
        rhi::DescriptorSetLayout::Descriptor desc = {};
        BindingAllocator alloc;

        ProcessResources(group.resources, group, alloc, desc);

        for (const auto &cond : group.conditionals) {
            if (activeConditions.count(std::string(cond.condition)) > 0) {
                ProcessResources(cond.resources, group, alloc, desc);
            }
        }

        return desc;
    }

} // namespace sky::sl
