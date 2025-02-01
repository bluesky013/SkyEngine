//
// Created by Zach Lee on 2023/9/5.
//

#include <render/RHI.h>
#include <render/RenderDefaultResource.h>
#include <rhi/Queue.h>
#include <rhi/Stream.h>

namespace sky {

    std::vector<uint8_t> DEFAULT_TEX2D_WHITE_DATA = {
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255
    };

    std::vector<uint8_t> DEFAULT_TEX2D_BLACK_DATA = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
    };

    static constexpr uint32_t MAX_SET_PER_POOL = 8;
    static std::vector<rhi::DescriptorSetPool::PoolSize> SIZES = {
        {rhi::DescriptorType::SAMPLER                , 4 * MAX_SET_PER_POOL},
        {rhi::DescriptorType::SAMPLED_IMAGE          , 4 * MAX_SET_PER_POOL},
        {rhi::DescriptorType::STORAGE_IMAGE          , 4 * MAX_SET_PER_POOL},
        {rhi::DescriptorType::UNIFORM_BUFFER         , 1 * MAX_SET_PER_POOL},
        {rhi::DescriptorType::STORAGE_BUFFER         , 4 * MAX_SET_PER_POOL},
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC , 1 * MAX_SET_PER_POOL},
        {rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC , 1 * MAX_SET_PER_POOL},
        {rhi::DescriptorType::INPUT_ATTACHMENT       , 4 * MAX_SET_PER_POOL},
    };

    void RenderDefaultResource::Init()
    {
        auto *device = RHI::Get()->GetDevice();

        rhi::DescriptorSetPool::Descriptor poolDesc = {};
        poolDesc.maxSets = MAX_SET_PER_POOL;
        poolDesc.sizeCount = static_cast<uint32_t>(SIZES.size());
        poolDesc.sizeData = SIZES.data();
        defaultPool = device->CreateDescriptorSetPool(poolDesc);
        emptyVI = device->CreateVertexInput({});

        auto emptyRHIDesLayout = device->CreateDescriptorSetLayout({});
        emptyDesLayout = new ResourceGroupLayout();
        emptyDesLayout->SetRHILayout(emptyRHIDesLayout);

        emptySet = new ResourceGroup();
        emptySet->Init(emptyDesLayout, *defaultPool);

        rhi::Sampler::Descriptor samplerDesc = {};
        samplerDesc.minFilter = rhi::Filter::LINEAR;
        samplerDesc.magFilter = rhi::Filter::LINEAR;
        samplerDesc.addressModeU = rhi::WrapMode::REPEAT;
        samplerDesc.addressModeV = rhi::WrapMode::REPEAT;
        samplerDesc.addressModeW = rhi::WrapMode::REPEAT;
        samplerDesc.maxLod = 13.f;
        defaultSampler = device->CreateSampler(samplerDesc);

        rhi::Image::Descriptor imageDesc = {};
        imageDesc.imageType = rhi::ImageType::IMAGE_2D;
        imageDesc.format = rhi::PixelFormat::RGBA8_UNORM;
        imageDesc.extent = {2, 2, 1};
        imageDesc.memory = rhi::MemoryType::GPU_ONLY;
        imageDesc.usage = rhi::ImageUsageFlagBit::SAMPLED | rhi::ImageUsageFlagBit::TRANSFER_DST;

        auto *queue = device->GetQueue(rhi::QueueType::TRANSFER);

        {
            texture2DWhite = new Texture2D();
            texture2DWhite->Init(rhi::PixelFormat::RGBA8_UNORM, 2, 2, 1);
            texture2DWhite->Upload(std::move(DEFAULT_TEX2D_WHITE_DATA), queue);
            texture2DWhite->Wait();
        }

        {
            texture2dBlack = new Texture2D();
            texture2dBlack->Init(rhi::PixelFormat::RGBA8_UNORM, 2, 2, 1);
            texture2dBlack->Upload(std::move(DEFAULT_TEX2D_BLACK_DATA), queue);
            texture2dBlack->Wait();
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
    }

    void RenderDefaultResource::Reset()
    {
        defaultPool = nullptr;
        emptyDesLayout = nullptr;
        emptySet = nullptr;
        defaultSampler = nullptr;

        texture2DWhite = nullptr;
        texture2dBlack = nullptr;
    }
} // namespace sky
