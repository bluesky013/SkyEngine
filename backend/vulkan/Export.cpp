//
// Created by Zach Lee on 2023/2/19.
//

#include "rhi/Instance.h"
#include "vulkan/Instance.h"

extern "C" SKY_EXPORT sky::rhi::Instance *CreateInstance()
{
    return new sky::vk::Instance();
}
