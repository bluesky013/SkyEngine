//
// Created on 2026/10/28.
//

#include <core/platform/Platform.h>

#if !SKY_PLATFORM_IOS
    #include "VulkanDeviceCapabilities.h"
    #include <algorithm>
    #include <gtest/gtest.h>
    #include <set>
    #include <string>

using namespace sky::aurora;

namespace {
    struct FeatureProbe {
        bool                         timeline        = true;
        bool                         synchronization = true;
        bool                         rendering       = true;
        uint32_t                     calls           = 0;
        std::vector<VkStructureType> structures;
    };
    struct FunctionProbe {
        std::vector<std::string> requested;
        std::set<std::string>    missing;
    };
    thread_local FeatureProbe  *features  = nullptr;
    thread_local FunctionProbe *functions = nullptr;

    void VKAPI_CALL QueryFeatures(VkPhysicalDevice, VkPhysicalDeviceFeatures2 *value)
    {
        ++features->calls;
        value->features.samplerAnisotropy = VK_TRUE;
        for (auto *entry = static_cast<VkBaseOutStructure *>(value->pNext); entry; entry = entry->pNext) {
            features->structures.push_back(entry->sType);
            switch (entry->sType) {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
                static_cast<VkPhysicalDeviceShaderDrawParametersFeatures *>(static_cast<void *>(entry))->shaderDrawParameters = VK_TRUE;
                break;
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
                static_cast<VkPhysicalDeviceTimelineSemaphoreFeatures *>(static_cast<void *>(entry))->timelineSemaphore = features->timeline;
                break;
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES:
                static_cast<VkPhysicalDeviceSynchronization2Features *>(static_cast<void *>(entry))->synchronization2 = features->synchronization;
                break;
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES:
                static_cast<VkPhysicalDeviceDynamicRenderingFeatures *>(static_cast<void *>(entry))->dynamicRendering = features->rendering;
                break;
            default: ADD_FAILURE() << "Unexpected feature structure " << entry->sType; break;
            }
        }
    }
    void VKAPI_CALL NativeEntry()
    {
    }
    PFN_vkVoidFunction VKAPI_CALL QueryFunction(VkDevice, const char *name)
    {
        functions->requested.emplace_back(name);
        return functions->missing.contains(name) ? nullptr : NativeEntry;
    }
    std::vector<VkExtensionProperties> Extensions(std::initializer_list<const char *> names)
    {
        std::vector<VkExtensionProperties> result;
        for (const auto *name : names) {
            VkExtensionProperties value{};
            std::strncpy(value.extensionName, name, sizeof(value.extensionName) - 1);
            result.push_back(value);
        }
        return result;
    }
    std::set<std::string> EnabledExtensions(const VulkanDeviceNegotiation &value)
    {
        return {value.extensions.begin(), value.extensions.end()};
    }
    class VulkanDeviceCapabilityTest : public ::testing::Test {
    protected:
        void SetUp() override
        {
            features  = &featureProbe;
            functions = &functionProbe;
        }
        void TearDown() override
        {
            features  = nullptr;
            functions = nullptr;
        }
        FeatureProbe  featureProbe;
        FunctionProbe functionProbe;
    };
} // namespace

TEST_F(VulkanDeviceCapabilityTest, Vulkan11QueriesAdvertisedKHRFeaturesAndUsesTheRenderPassCompiler)
{
    const auto              extensions = Extensions({VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
                                                     VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME});
    VulkanDeviceNegotiation negotiated;
    ASSERT_EQ(NegotiateVulkanDevice(QueryFeatures, VK_NULL_HANDLE, VK_API_VERSION_1_1, extensions, negotiated), nullptr);
    EXPECT_FALSE(negotiated.dynamicRendering);
    EXPECT_TRUE(negotiated.renderPass2);
    EXPECT_EQ(negotiated.draw.shaderDrawParameters, VK_TRUE);
    EXPECT_EQ(negotiated.core.features.samplerAnisotropy, VK_TRUE);
    EXPECT_EQ(featureProbe.structures, (std::vector<VkStructureType>{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
                                                                     VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
                                                                     VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES}));
    EXPECT_EQ(EnabledExtensions(negotiated),
              (std::set<std::string>{VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
                                     VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME}));
    EXPECT_EQ(negotiated.synchronization.pNext, nullptr);
    EXPECT_EQ(SelectVulkanCompiler(VulkanFrameGraphMode::AUTO, false, true), VulkanFrameGraphMode::RENDER_PASS);
    EXPECT_FALSE(SelectVulkanCompiler(VulkanFrameGraphMode::DYNAMIC_RENDERING, false, true));
}

TEST_F(VulkanDeviceCapabilityTest, Core13DoesNotRequirePromotedExtensionNames)
{
    VulkanDeviceNegotiation negotiated;
    ASSERT_EQ(NegotiateVulkanDevice(QueryFeatures, VK_NULL_HANDLE, VK_API_VERSION_1_3, {}, negotiated), nullptr);
    EXPECT_TRUE(negotiated.dynamicRendering);
    EXPECT_TRUE(negotiated.renderPass2);
    EXPECT_TRUE(negotiated.extensions.empty());
    EXPECT_EQ(featureProbe.structures.size(), 4U);
    EXPECT_EQ(featureProbe.structures.back(), VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES);
    EXPECT_EQ(SelectVulkanCompiler(VulkanFrameGraphMode::AUTO, true, true), VulkanFrameGraphMode::DYNAMIC_RENDERING);
    EXPECT_EQ(SelectVulkanCompiler(VulkanFrameGraphMode::RENDER_PASS, true, true), VulkanFrameGraphMode::RENDER_PASS);
}

TEST_F(VulkanDeviceCapabilityTest, Core12UsesItsCoreTimelineAndRenderPassWithKHRSyncAndRendering)
{
    const auto              extensions = Extensions({VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME});
    VulkanDeviceNegotiation negotiated;
    ASSERT_EQ(NegotiateVulkanDevice(QueryFeatures, VK_NULL_HANDLE, VK_API_VERSION_1_2, extensions, negotiated), nullptr);
    EXPECT_TRUE(negotiated.dynamicRendering);
    EXPECT_EQ(EnabledExtensions(negotiated),
              (std::set<std::string>{VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME}));
}

TEST_F(VulkanDeviceCapabilityTest, Vulkan11DynamicRenderingEnablesItsAdvertisedDependencies)
{
    const auto extensions =
        Extensions({VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
                    VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME});
    VulkanDeviceNegotiation negotiated;
    ASSERT_EQ(NegotiateVulkanDevice(QueryFeatures, VK_NULL_HANDLE, VK_API_VERSION_1_1, extensions, negotiated), nullptr);
    EXPECT_TRUE(negotiated.dynamicRendering);
    EXPECT_EQ(EnabledExtensions(negotiated).size(), extensions.size());
    EXPECT_EQ(negotiated.synchronization.pNext, &negotiated.rendering);
}

TEST_F(VulkanDeviceCapabilityTest, MissingDynamicDependencyKeepsOnlyTheAvailableRenderPassPath)
{
    const auto              extensions = Extensions({VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
                                                     VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME});
    VulkanDeviceNegotiation negotiated;
    ASSERT_EQ(NegotiateVulkanDevice(QueryFeatures, VK_NULL_HANDLE, VK_API_VERSION_1_1, extensions, negotiated), nullptr);
    EXPECT_FALSE(negotiated.dynamicRendering);
    EXPECT_EQ(featureProbe.structures.size(), 3U);
    EXPECT_FALSE(EnabledExtensions(negotiated).contains(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME));
}

TEST_F(VulkanDeviceCapabilityTest, AdvertisedButFalseDynamicFeatureIsNotEnabled)
{
    featureProbe.rendering             = false;
    const auto              extensions = Extensions({VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME});
    VulkanDeviceNegotiation negotiated;
    ASSERT_EQ(NegotiateVulkanDevice(QueryFeatures, VK_NULL_HANDLE, VK_API_VERSION_1_2, extensions, negotiated), nullptr);
    EXPECT_FALSE(negotiated.dynamicRendering);
    EXPECT_EQ(featureProbe.structures.size(), 4U);
    EXPECT_EQ(negotiated.synchronization.pNext, nullptr);
    EXPECT_EQ(EnabledExtensions(negotiated), (std::set<std::string>{VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME}));
    EXPECT_EQ(SelectVulkanCompiler(VulkanFrameGraphMode::AUTO, false, true), VulkanFrameGraphMode::RENDER_PASS);
}

TEST_F(VulkanDeviceCapabilityTest, MissingRequiredExtensionFailsBeforeAnyFeatureQuery)
{
    for (const auto *missing :
         {VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME}) {
        auto extensions = Extensions(
            {VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME});
        std::erase_if(extensions, [&](const auto &extension) { return std::strcmp(extension.extensionName, missing) == 0; });
        VulkanDeviceNegotiation negotiated;
        EXPECT_NE(NegotiateVulkanDevice(QueryFeatures, VK_NULL_HANDLE, VK_API_VERSION_1_1, extensions, negotiated), nullptr);
    }
    EXPECT_EQ(featureProbe.calls, 0U);
}

TEST_F(VulkanDeviceCapabilityTest, RequiredFeaturesCannotBeInventedFromExtensionNames)
{
    const auto extensions =
        Extensions({VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME});
    for (const bool missingTimeline : {false, true}) {
        featureProbe.timeline        = !missingTimeline;
        featureProbe.synchronization = missingTimeline;
        VulkanDeviceNegotiation negotiated;
        EXPECT_NE(NegotiateVulkanDevice(QueryFeatures, VK_NULL_HANDLE, VK_API_VERSION_1_1, extensions, negotiated), nullptr);
        EXPECT_TRUE(negotiated.extensions.empty());
    }
}

TEST_F(VulkanDeviceCapabilityTest, Vulkan10OrMissingFeatureQueryIsRejected)
{
    VulkanDeviceNegotiation oldDevice, missingQuery;
    EXPECT_NE(NegotiateVulkanDevice(QueryFeatures, VK_NULL_HANDLE, VK_API_VERSION_1_0, {}, oldDevice), nullptr);
    EXPECT_NE(NegotiateVulkanDevice(nullptr, VK_NULL_HANDLE, VK_API_VERSION_1_3, {}, missingQuery), nullptr);
    EXPECT_EQ(featureProbe.calls, 0U);
}

TEST_F(VulkanDeviceCapabilityTest, Vulkan11UsesKHRCommandsEvenWhenCoreTrampolinesExist)
{
    VulkanDeviceFunctions fn;
    fn.vkCmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(NativeEntry);
    LoadVulkanPromotedFunctions(QueryFunction, VK_NULL_HANDLE, VK_API_VERSION_1_1, false, true, fn);
    ASSERT_EQ(functionProbe.requested.size(), 9U);
    EXPECT_TRUE(HasVulkanRequiredFunctions(fn, false, true));
    EXPECT_EQ(fn.vkCmdBeginRendering, nullptr);
    EXPECT_EQ(fn.vkCmdEndRendering, nullptr);
    for (const auto &name : functionProbe.requested)
        EXPECT_TRUE(name.ends_with("KHR")) << name;
    EXPECT_EQ(std::count(functionProbe.requested.begin(), functionProbe.requested.end(), "vkQueueSubmit2KHR"), 1);
    EXPECT_EQ(std::count(functionProbe.requested.begin(), functionProbe.requested.end(), "vkGetSemaphoreCounterValueKHR"), 1);
    EXPECT_EQ(std::count(functionProbe.requested.begin(), functionProbe.requested.end(), "vkCreateRenderPass2KHR"), 1);
}

TEST_F(VulkanDeviceCapabilityTest, Vulkan13UsesCoreCommandsWithoutRequiringKHRExports)
{
    VulkanDeviceFunctions fn;
    LoadVulkanPromotedFunctions(QueryFunction, VK_NULL_HANDLE, VK_API_VERSION_1_3, true, true, fn);
    ASSERT_EQ(functionProbe.requested.size(), 11U);
    EXPECT_TRUE(HasVulkanRequiredFunctions(fn, true, true));
    for (const auto &name : functionProbe.requested)
        EXPECT_FALSE(name.ends_with("KHR")) << name;
}

TEST_F(VulkanDeviceCapabilityTest, Vulkan12LoadsTheCorrectMixedCommandFamilies)
{
    VulkanDeviceFunctions fn;
    LoadVulkanPromotedFunctions(QueryFunction, VK_NULL_HANDLE, VK_API_VERSION_1_2, true, true, fn);
    EXPECT_TRUE(HasVulkanRequiredFunctions(fn, true, true));
    EXPECT_EQ(std::count(functionProbe.requested.begin(), functionProbe.requested.end(), "vkGetSemaphoreCounterValue"), 1);
    EXPECT_EQ(std::count(functionProbe.requested.begin(), functionProbe.requested.end(), "vkCreateRenderPass2"), 1);
    EXPECT_EQ(std::count(functionProbe.requested.begin(), functionProbe.requested.end(), "vkQueueSubmit2KHR"), 1);
    EXPECT_EQ(std::count(functionProbe.requested.begin(), functionProbe.requested.end(), "vkCmdBeginRenderingKHR"), 1);
}

TEST_F(VulkanDeviceCapabilityTest, AnEnabledFeatureWithAMissingRequiredCommandIsRejected)
{
    for (const auto *missing : {"vkQueueSubmit2KHR", "vkCmdPipelineBarrier2KHR", "vkGetSemaphoreCounterValueKHR", "vkWaitSemaphoresKHR",
                                "vkSignalSemaphoreKHR", "vkCreateRenderPass2KHR", "vkCmdBeginRenderPass2KHR", "vkCmdNextSubpass2KHR",
                                "vkCmdEndRenderPass2KHR", "vkCmdBeginRenderingKHR", "vkCmdEndRenderingKHR"}) {
        functionProbe.missing = {missing};
        VulkanDeviceFunctions fn;
        LoadVulkanPromotedFunctions(QueryFunction, VK_NULL_HANDLE, VK_API_VERSION_1_1, true, true, fn);
        EXPECT_FALSE(HasVulkanRequiredFunctions(fn, true, true)) << missing;
    }
}

TEST_F(VulkanDeviceCapabilityTest, ExplicitCompilerChoicesNeverSilentlyChange)
{
    EXPECT_EQ(SelectVulkanCompiler(VulkanFrameGraphMode::DYNAMIC_RENDERING, true, true), VulkanFrameGraphMode::DYNAMIC_RENDERING);
    EXPECT_FALSE(SelectVulkanCompiler(VulkanFrameGraphMode::DYNAMIC_RENDERING, false, true));
    EXPECT_EQ(SelectVulkanCompiler(VulkanFrameGraphMode::RENDER_PASS, true, true), VulkanFrameGraphMode::RENDER_PASS);
    EXPECT_FALSE(SelectVulkanCompiler(VulkanFrameGraphMode::RENDER_PASS, true, false));
    EXPECT_FALSE(SelectVulkanCompiler(VulkanFrameGraphMode::AUTO, false, false));
    EXPECT_FALSE(SelectVulkanCompiler(static_cast<VulkanFrameGraphMode>(255), true, true));
}
#endif
