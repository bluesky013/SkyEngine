//
// Created by Zach Lee on 2021/11/7.
//
#include "vulkan/Driver.h"
#include "vulkan/Buffer.h"
#include "vulkan/Image.h"

int main()
{
    using namespace sky::drv;

    auto driver = Driver::Create({"", "", true});

    auto device = driver->CreateDevice({});

    Buffer::Descriptor bufferDes = {};
    bufferDes.size   = 128;
    bufferDes.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
    bufferDes.usage  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    auto buffer = device->CreateDeviceObject<Buffer>(bufferDes);

    Image::Descriptor imageDes = {};
    imageDes.imageType   = VK_IMAGE_TYPE_2D;
    imageDes.format      = VK_FORMAT_R8G8B8A8_UNORM;
    imageDes.extent      = {4, 4, 1};
    imageDes.mipLevels   = 1;
    imageDes.arrayLayers = 1;
    imageDes.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageDes.samples     = VK_SAMPLE_COUNT_1_BIT;
    imageDes.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imageDes.memory      = VMA_MEMORY_USAGE_GPU_ONLY;
    auto image = device->CreateDeviceObject<Image>(imageDes);


    delete buffer;
    delete image;
    delete device;
    Driver::Destroy(driver);
    return 0;
}