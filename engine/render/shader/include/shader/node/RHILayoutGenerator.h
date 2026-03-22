//
// Created by blues on 2025/4/21.
//

#pragma once

#include <shader/node/ResourceGroupDecl.h>
#include <rhi/DescriptorSetLayout.h>
#include <set>
#include <string>

namespace sky::sl {

    /// Generates rhi::DescriptorSetLayout::Descriptor from a ResourceGroupDecl.
    ///
    /// Produces the same binding layout that shader reflection would give:
    ///   - Binding numbers match [[vk::binding]] (flat, sequential)
    ///   - DescriptorType is mapped from node types
    ///   - Visibility from node or group default
    class RHILayoutGenerator {
    public:
        /// Generate layout including all conditional blocks (superset).
        static rhi::DescriptorSetLayout::Descriptor Generate(
            const ResourceGroupDecl &group);

        /// Generate layout with only specific conditions active.
        static rhi::DescriptorSetLayout::Descriptor GenerateWithConditions(
            const ResourceGroupDecl &group,
            const std::set<std::string> &activeConditions);
    };

} // namespace sky::sl
