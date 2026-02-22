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

        rhi::ImageUploadRequest whiteData = {
            new rhi::RawPtrStream(DEFAULT_TEX2D_WHITE_DATA.data()),
            0, DEFAULT_TEX2D_WHITE_DATA.size(), 0, 0
        };

        rhi::ImageUploadRequest blackData = {
            new rhi::RawPtrStream(DEFAULT_TEX2D_BLACK_DATA.data()),
            0, DEFAULT_TEX2D_BLACK_DATA.size(), 0, 0
        };
        {
            texture2DWhite = new Texture2D();
            texture2DWhite->Init(rhi::PixelFormat::RGBA8_UNORM, 2, 2, 1);
            texture2DWhite->SetUploadStream(ImageData{{whiteData}});
            texture2DWhite->Upload(queue);
            texture2DWhite->Wait();
        }

        {
            texture2DBlack = new Texture2D();
            texture2DBlack->Init(rhi::PixelFormat::RGBA8_UNORM, 2, 2, 1);
            texture2DWhite->SetUploadStream(ImageData{{whiteData}});
            texture2DWhite->Upload(queue);
            texture2DBlack->Wait();
        }

        {
            textureCubeWhite = new TextureCube();
            textureCubeWhite->Init(rhi::PixelFormat::RGBA8_UNORM, 2, 2, 1);

            ImageData imageData;
            for (uint32_t i = 0; i < 6; ++i) {
                whiteData.layer = i;
                imageData.slices.emplace_back(whiteData);
            }
            textureCubeWhite->SetUploadStream(std::move(imageData));
            textureCubeWhite->Upload(queue);
            textureCubeWhite->Wait();
        }
    }

    void RenderDefaultResource::Reset()
    {
        defaultPool = nullptr;
        emptyDesLayout = nullptr;
        emptySet = nullptr;
        defaultSampler = nullptr;

        texture2DWhite = nullptr;
        texture2DBlack = nullptr;
        textureCubeWhite = nullptr;
    }
} // namespace sky
