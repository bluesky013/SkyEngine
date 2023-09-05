//
// Created by Zach Lee on 2023/9/5.
//

#include <render/RenderDefaultResource.h>
#include <render/RHI.h>
#include <rhi/Queue.h>

namespace sky {

    const uint8_t DEFAULT_TEX_DATA[] = {
        255, 255, 255, 255, 127, 127, 127, 255,
        127, 127, 127, 255, 255, 255, 255, 255
    };

    void RenderDefaultResource::Init()
    {
        auto *device = RHI::Get()->GetDevice();
        defaultPool = device->CreateDescriptorSetPool({});
        emptyDesLayout = device->CreateDescriptorSetLayout({});
        defaultSampler = device->CreateSampler({});

        rhi::Image::Descriptor imageDesc = {};
        imageDesc.imageType = rhi::ImageType::IMAGE_2D;
        imageDesc.format = rhi::PixelFormat::RGBA8_UNORM;
        imageDesc.extent = {2, 2, 1};
        imageDesc.memory = rhi::MemoryType::GPU_ONLY;
        imageDesc.usage = rhi::ImageUsageFlagBit::SAMPLED | rhi::ImageUsageFlagBit::TRANSFER_DST;

        auto *queue = device->GetQueue(rhi::QueueType::TRANSFER);
        rhi::TransferTaskHandle taskHandle = 0;

        rhi::ImageUploadRequest request = {};
        request.data = DEFAULT_TEX_DATA;
        request.size = sizeof(DEFAULT_TEX_DATA);
        request.imageOffset = {0, 0, 0};
        request.imageExtent = imageDesc.extent;

        {
            rhi::ImageViewDesc viewDesc = {};
            imageDesc.arrayLayers = 1;
            viewDesc.subRange.layers = 1;
            viewDesc.viewType = rhi::ImageViewType::VIEW_2D;

            auto image = device->CreateImage(imageDesc);
            texture2D = image->CreateView(viewDesc);
            taskHandle = queue->UploadImage(image, request);
        }

//        {
//            rhi::ImageViewDesc viewDesc = {};
//            imageDesc.arrayLayers = 6;
//            viewDesc.subRange.layers = 6;
//            viewDesc.viewType = rhi::ImageViewType::VIEW_CUBE;
//
//            auto image = device->CreateImage(imageDesc);
//            textureCube = image->CreateView(viewDesc);
//            std::vector<rhi::ImageUploadRequest> requests;
//            for (uint32_t i = 0; i < 6; ++i) {
//                request.layer = i;
//                requests.emplace_back(request);
//            }
//            taskHandle = queue->UploadImage(image, requests);
//        }

        queue->Wait(taskHandle);
    }

    void RenderDefaultResource::Reset()
    {
        defaultPool = nullptr;
        emptyDesLayout = nullptr;
        defaultSampler = nullptr;

        texture2D = nullptr;
        textureCube = nullptr;
    }
} // namespace sky
