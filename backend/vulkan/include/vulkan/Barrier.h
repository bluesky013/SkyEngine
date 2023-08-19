//
// Created by Zach Lee on 2023/5/13.
//

#pragma once

#include <vector>
#include <vulkan/Basic.h>
#include <rhi/Core.h>

namespace sky::vk {
    class Device;

    void ValidateAccessInfoMapByExtension(const std::vector<VkExtensionProperties>& extensions);
    AccessInfo GetAccessInfo(const rhi::AccessFlags &flags);

} // namespace sky::vk