//
// Created on 2026/10/28.
//

#pragma once

#include <VulkanFunctions.h>
#include <aurora/rdg/DeviceFrameContext.h>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace sky::aurora {
    inline bool HasVulkanDeviceExtension(std::span<const VkExtensionProperties> extensions, const char *name)
    {
        for (const auto &extension : extensions)
            if (std::strcmp(extension.extensionName, name) == 0)
                return true;
        return false;
    }

    struct VulkanDeviceNegotiation {
        VkPhysicalDeviceFeatures2                    core{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceShaderDrawParametersFeatures draw{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES};
        VkPhysicalDeviceTimelineSemaphoreFeatures    timeline{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
        VkPhysicalDeviceSynchronization2Features     synchronization{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};
        VkPhysicalDeviceDynamicRenderingFeatures     rendering{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
        std::vector<const char *>                    extensions;
        bool                                         renderPass2      = false;
        bool                                         dynamicRendering = false;

        VulkanDeviceNegotiation()                                           = default;
        VulkanDeviceNegotiation(const VulkanDeviceNegotiation &)            = delete;
        VulkanDeviceNegotiation &operator=(const VulkanDeviceNegotiation &) = delete;
    };

    inline const char *NegotiateVulkanDevice(PFN_vkGetPhysicalDeviceFeatures2       query,
                                             VkPhysicalDevice                       gpu,
                                             uint32_t                               apiVersion,
                                             std::span<const VkExtensionProperties> extensions,
                                             VulkanDeviceNegotiation               &result)
    {
        if (apiVersion < VK_API_VERSION_1_1 || !query)
            return "AuroraVulkan requires Vulkan 1.1 feature queries";
        const bool core12 = apiVersion >= VK_API_VERSION_1_2;
        const bool core13 = apiVersion >= VK_API_VERSION_1_3;
        const auto has    = [&](const char *name) { return HasVulkanDeviceExtension(extensions, name); };
        if (!core12 && !has(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME))
            return "AuroraVulkan requires timeline semaphores (core 1.2 or VK_KHR_timeline_semaphore)";
        if (!core13 && !has(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME))
            return "AuroraVulkan requires synchronization2 (core 1.3 or VK_KHR_synchronization2)";
        result.renderPass2        = core12 || has(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
        const bool depthResolve   = core12 || (result.renderPass2 && has(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME));
        const bool queryRendering = core13 || (depthResolve && has(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME));
        if (!result.renderPass2 && !queryRendering)
            return "AuroraVulkan requires render-pass2 or dynamic rendering";

        result.core.pNext            = &result.draw;
        result.draw.pNext            = &result.timeline;
        result.timeline.pNext        = &result.synchronization;
        result.synchronization.pNext = queryRendering ? &result.rendering : nullptr;
        query(gpu, &result.core);
        if (!result.timeline.timelineSemaphore)
            return "Selected GPU does not support timelineSemaphore";
        if (!result.synchronization.synchronization2)
            return "Selected GPU does not support synchronization2";
        result.dynamicRendering = queryRendering && result.rendering.dynamicRendering == VK_TRUE;
        if (!result.renderPass2 && !result.dynamicRendering)
            return "Selected GPU supports neither render-pass2 nor dynamic rendering";

        if (!core12) {
            result.extensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
            if (result.renderPass2)
                result.extensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
            if (depthResolve)
                result.extensions.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
        }
        if (!core13) {
            result.extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
            if (result.dynamicRendering)
                result.extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        }
        // Unsupported optional feature structures must not enter the device enable chain.
        result.synchronization.pNext = result.dynamicRendering ? &result.rendering : nullptr;
        return nullptr;
    }

    inline void LoadVulkanPromotedFunctions(
        PFN_vkGetDeviceProcAddr query, VkDevice device, uint32_t apiVersion, bool dynamicRendering, bool renderPass2, VulkanDeviceFunctions &fn)
    {
#define LOAD_PROMOTED(name, version) fn.name = reinterpret_cast<PFN_##name>(query(device, apiVersion >= version ? #name : #name "KHR"))
        LOAD_PROMOTED(vkGetSemaphoreCounterValue, VK_API_VERSION_1_2);
        LOAD_PROMOTED(vkWaitSemaphores, VK_API_VERSION_1_2);
        LOAD_PROMOTED(vkSignalSemaphore, VK_API_VERSION_1_2);
        LOAD_PROMOTED(vkQueueSubmit2, VK_API_VERSION_1_3);
        LOAD_PROMOTED(vkCmdPipelineBarrier2, VK_API_VERSION_1_3);
        if (dynamicRendering) {
            LOAD_PROMOTED(vkCmdBeginRendering, VK_API_VERSION_1_3);
            LOAD_PROMOTED(vkCmdEndRendering, VK_API_VERSION_1_3);
        } else {
            fn.vkCmdBeginRendering = nullptr;
            fn.vkCmdEndRendering   = nullptr;
        }
        if (renderPass2) {
            LOAD_PROMOTED(vkCreateRenderPass2, VK_API_VERSION_1_2);
            LOAD_PROMOTED(vkCmdBeginRenderPass2, VK_API_VERSION_1_2);
            LOAD_PROMOTED(vkCmdNextSubpass2, VK_API_VERSION_1_2);
            LOAD_PROMOTED(vkCmdEndRenderPass2, VK_API_VERSION_1_2);
        } else {
            fn.vkCreateRenderPass2   = nullptr;
            fn.vkCmdBeginRenderPass2 = nullptr;
            fn.vkCmdNextSubpass2     = nullptr;
            fn.vkCmdEndRenderPass2   = nullptr;
        }
#undef LOAD_PROMOTED
    }

    inline bool HasVulkanRequiredFunctions(const VulkanDeviceFunctions &fn, bool dynamicRendering, bool renderPass2)
    {
        return fn.vkGetSemaphoreCounterValue && fn.vkWaitSemaphores && fn.vkSignalSemaphore && fn.vkQueueSubmit2 && fn.vkCmdPipelineBarrier2 &&
               (!dynamicRendering || (fn.vkCmdBeginRendering && fn.vkCmdEndRendering)) &&
               (!renderPass2 || (fn.vkCreateRenderPass2 && fn.vkCmdBeginRenderPass2 && fn.vkCmdNextSubpass2 && fn.vkCmdEndRenderPass2));
    }

    inline std::optional<VulkanFrameGraphMode> SelectVulkanCompiler(VulkanFrameGraphMode requested, bool dynamicRendering, bool renderPass2)
    {
        switch (requested) {
        case VulkanFrameGraphMode::AUTO:
            if (dynamicRendering)
                return VulkanFrameGraphMode::DYNAMIC_RENDERING;
            if (renderPass2)
                return VulkanFrameGraphMode::RENDER_PASS;
            break;
        case VulkanFrameGraphMode::DYNAMIC_RENDERING:
            if (dynamicRendering)
                return requested;
            break;
        case VulkanFrameGraphMode::RENDER_PASS:
            if (renderPass2)
                return requested;
            break;
        }
        return std::nullopt;
    }
} // namespace sky::aurora
