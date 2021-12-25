//
// Created by Zach Lee on 2021/12/22.
//

#pragma once

#include <vulkan/Image.h>

namespace sky {

    class GraphAttachment {
    public:
        GraphAttachment() = default;
        ~GraphAttachment() = default;

        struct Config {
            bool fromParent = true;
            VkImageType imageType   = VK_IMAGE_TYPE_2D;
            VkFormat    format      = VK_FORMAT_UNDEFINED;
            VkExtent3D  extent      = {1, 1, 1};
            uint32_t    mipLevels   = 1;
            uint32_t    arrayLayers = 1;
        };

    };

}