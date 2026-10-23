#include <VulkanBindingPlan.h>

#include <gtest/gtest.h>

using namespace sky::aurora;

namespace {
    ShaderReflection PlanReflection()
    {
        ShaderReflection reflection;
        reflection.kind         = ShaderReflectionKind::PROGRAM;
        reflection.complete     = true;
        reflection.bindingModel = ShaderBindingModel::VULKAN_DESCRIPTORS;
        reflection.stages       = ShaderStageFlagBit::VS | ShaderStageFlagBit::FS;
        ShaderResource image{"images", ShaderResourceType::SAMPLED_IMAGE, 0, 4, 4};
        image.declarationOwner             = "test/Shader.slang";
        image.descriptorArray              = true;
        image.stages                       = reflection.stages;
        reflection.resources               = {image};
        reflection.declarations            = reflection.resources;
        reflection.nativeBindings          = {{0, 4, ShaderStageFlagBit::VS, 12}, {0, 4, ShaderStageFlagBit::FS, 7}};
        reflection.nativeBindings[0].space = 1;
        reflection.nativeBindings[1].space = 3;
        for (auto &binding : reflection.nativeBindings) {
            binding.category = ShaderNativeCategory::DESCRIPTOR;
            binding.count    = 4;
        }
        return reflection;
    }
} // namespace

TEST(VulkanBindingPlan, SparseStageAddressesFanOutFromOneLogicalBinding)
{
    const auto        reflection = PlanReflection();
    VulkanBindingPlan plan;
    std::string       error;
    ASSERT_TRUE(BuildVulkanBindingPlan(reflection, 8, plan, error)) << error;
    ASSERT_EQ(plan.sets.size(), 2u);
    EXPECT_EQ(plan.sets.at(1).bindings[0].physicalBinding, 12u);
    EXPECT_EQ(plan.sets.at(3).bindings[0].physicalBinding, 7u);
    EXPECT_EQ(plan.sets.at(1).bindings[0].logicalBinding, 4u);
    EXPECT_EQ(plan.sets.at(3).bindings[0].logicalBinding, 4u);
    EXPECT_EQ(plan.sets.at(1).bindings[0].slotBase, plan.sets.at(3).bindings[0].slotBase);
    EXPECT_EQ(plan.logicalSlotCounts.at(0), 4u);
}

TEST(VulkanBindingPlan, SharedStageAddressMergesAndArraysDoNotOccupyAdjacentBindingNumbers)
{
    auto reflection                    = PlanReflection();
    reflection.nativeBindings[1].space = 1;
    reflection.nativeBindings[1].index = 12;
    ShaderResource sampler{"sampler", ShaderResourceType::SAMPLER, 0, 5, 1};
    sampler.stages           = ShaderStageFlags(ShaderStageFlagBit::VS);
    sampler.declarationOwner = "test/Shader.slang";
    reflection.resources.push_back(sampler);
    reflection.declarations.push_back(sampler);
    ShaderNativeBinding binding{0, 5, ShaderStageFlagBit::VS, 13};
    binding.category = ShaderNativeCategory::DESCRIPTOR;
    binding.space    = 1;
    reflection.nativeBindings.push_back(binding);
    VulkanBindingPlan plan;
    std::string       error;
    ASSERT_TRUE(BuildVulkanBindingPlan(reflection, 8, plan, error)) << error;
    ASSERT_EQ(plan.sets.at(1).bindings.size(), 2u);
    EXPECT_EQ(plan.sets.at(1).bindings[0].stages, reflection.stages);
    EXPECT_EQ(plan.sets.at(1).bindings[0].count, 4u);
    EXPECT_EQ(plan.sets.at(1).bindings[1].physicalBinding, 13u);
}

TEST(VulkanBindingPlan, DynamicOffsetsFollowPhysicalOrderWithLogicalSources)
{
    ShaderReflection reflection;
    reflection.kind         = ShaderReflectionKind::PROGRAM;
    reflection.complete     = true;
    reflection.bindingModel = ShaderBindingModel::VULKAN_DESCRIPTORS;
    reflection.stages       = ShaderStageFlags(ShaderStageFlagBit::VS);
    for (const auto logicalBinding : {1u, 8u}) {
        ShaderResource resource{"block" + std::to_string(logicalBinding), ShaderResourceType::UNIFORM_BUFFER_DYNAMIC, 0, logicalBinding, 1};
        resource.stages           = reflection.stages;
        resource.declarationOwner = "test/Shader.slang";
        reflection.resources.push_back(resource);
        reflection.declarations.push_back(resource);
        ShaderBlockLayout block;
        block.name    = resource.name;
        block.binding = logicalBinding;
        block.size    = 16;
        block.members = {{"value", 0, 16, ShaderScalarType::FLOAT, ShaderTypeKind::VECTOR, 1, 4}};
        reflection.blocks.push_back(block);
        ShaderNativeBinding native{0, logicalBinding, ShaderStageFlagBit::VS, logicalBinding == 1 ? 7u : 0u};
        native.category = ShaderNativeCategory::DESCRIPTOR;
        native.space    = 2;
        reflection.nativeBindings.push_back(native);
    }
    VulkanBindingPlan plan;
    std::string       error;
    ASSERT_TRUE(BuildVulkanBindingPlan(reflection, 8, plan, error)) << error;
    EXPECT_EQ(plan.logicalDynamicCounts.at(0), 2u);
    EXPECT_EQ(plan.sets.at(2).dynamicOffsets, (std::vector<uint32_t>{1, 0}));
}

TEST(VulkanBindingPlan, MissingWrongCategoryOrConflictingAddressIsRejected)
{
    const auto original = PlanReflection();
    for (uint32_t fault = 0; fault < 4; ++fault) {
        auto reflection = original;
        if (fault == 0) {
            reflection.nativeBindings.pop_back();
        }
        if (fault == 1) {
            reflection.nativeBindings[0].category = ShaderNativeCategory::METAL_TEXTURE;
        }
        if (fault == 2) {
            reflection.nativeBindings[0].space = 8;
        }
        if (fault == 3) {
            auto resource    = reflection.resources[0];
            resource.name    = "other";
            resource.binding = 6;
            reflection.resources.push_back(resource);
            reflection.declarations.push_back(resource);
            auto vertex    = reflection.nativeBindings[0];
            vertex.binding = 6;
            reflection.nativeBindings.push_back(vertex);
            auto fragment    = reflection.nativeBindings[1];
            fragment.binding = 6;
            reflection.nativeBindings.push_back(fragment);
        }
        VulkanBindingPlan output;
        std::string       error;
        EXPECT_FALSE(BuildVulkanBindingPlan(reflection, 8, output, error)) << fault;
        EXPECT_FALSE(error.empty());
        EXPECT_TRUE(output.sets.empty());
    }
}

TEST(VulkanBindingPlan, MultipleLogicalGroupsInOnePhysicalSetAreExplicitlyUnsupported)
{
    auto reflection  = PlanReflection();
    auto resource    = reflection.resources[0];
    resource.set     = 1;
    resource.binding = 6;
    resource.name    = "other";
    reflection.resources.push_back(resource);
    reflection.declarations.push_back(resource);
    auto vertex    = reflection.nativeBindings[0];
    vertex.set     = 1;
    vertex.binding = 6;
    vertex.index   = 20;
    reflection.nativeBindings.push_back(vertex);
    auto fragment    = reflection.nativeBindings[1];
    fragment.set     = 1;
    fragment.binding = 6;
    fragment.index   = 21;
    reflection.nativeBindings.push_back(fragment);
    VulkanBindingPlan plan;
    std::string       error;
    EXPECT_FALSE(BuildVulkanBindingPlan(reflection, 8, plan, error));
    EXPECT_NE(error.find("multiple logical"), std::string::npos);
}
