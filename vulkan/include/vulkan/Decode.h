//
// Created by Zach Lee on 2022/10/23.
//

#pragma once

#include <vector>
#include <vulkan/Basic.h>
#include <vulkan/Image.h>

namespace sky::vk {

    ImageFormatInfo *GetImageInfoByFormat(VkFormat format);

    void ProcessASTC(uint8_t *input, uint64_t size, Image::Descriptor &imageDesc, std::vector<ImageUploadRequest> &requests);

    void ProcessDDS(uint8_t *input, uint64_t size, Image::Descriptor &imageDesc, std::vector<ImageUploadRequest> &requests);
}
