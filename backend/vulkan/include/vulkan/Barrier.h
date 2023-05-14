//
// Created by Zach Lee on 2023/5/13.
//

#pragma once

#include <vector>
#include <vulkan/Basic.h>
#include <rhi/Core.h>

namespace sky::vk {

    void ValidateAccessInfoMapByExtension(const std::vector<VkExtensionProperties>& extensions);
    AccessInfo GetAccessInfo(const std::vector<rhi::AccessFlag> &accesses, bool ignoreLayout = false);

} // namespace sky::vk