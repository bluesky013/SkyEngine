
//
// Created by Zach Lee on 2023/1/1.
//

#include "VulkanTerrainVTSample.h"
#include <EngineRoot.h>
#include <sstream>
#include <core/math/Matrix4.h>

#define KHRONOS_STATIC
#include <ktx.h>

//#define STATIC_BINDING 0

namespace sky {
    const uint32_t widthNum = 16;
    const uint32_t heightNum = 16;
    const uint32_t blockWidth = 256;
    const uint32_t blockHeight = 256;
    const uint32_t terrainWidth = 1024;
    const uint32_t terrainHeight = 1024;

    template <typename T>
    T &Visit(std::vector<T> &data, uint32_t stride, uint32_t x, uint32_t y)
    {
        return data[stride * y + x];
    }

    void VulkanTerrainVTSample::InitFeature()
    {
        deviceInfo.feature.sparseBinding = true;
    }

    void VulkanTerrainVTSample::OnTick(float delta)
    {

        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Wait();
        PlayerUpdate(delta);
        UpdateBinding();
        UpdateTerrainData();

        commandBuffer->Begin();

        auto cmd             = commandBuffer->GetNativeHandle();
        auto graphicsEncoder = commandBuffer->EncodeGraphics();

        VkClearValue clearValue     = {};
        clearValue.color.float32[0] = 0.f;
        clearValue.color.float32[1] = 0.f;
        clearValue.color.float32[2] = 0.f;
        clearValue.color.float32[3] = 1.f;

        vk::CmdDraw args          = {};
        args.type                 = vk::CmdDrawType::LINEAR;
        args.linear.firstVertex   = 0;
        args.linear.firstInstance = 0;
        args.linear.instanceCount = 1;

        vk::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer       = frameBuffers[imageIndex];
        beginInfo.renderPass        = renderPass;
        beginInfo.clearValueCount   = 1;
        beginInfo.clearValues       = &clearValue;

        graphicsEncoder.BeginPass(beginInfo);
        graphicsEncoder.BindPipeline(pso);
        graphicsEncoder.BindShaderResource(terrain.setBinder);
        args.linear.vertexCount   = 6;
        graphicsEncoder.DrawLinear(args.linear);

        graphicsEncoder.BindPipeline(playerPso);
        graphicsEncoder.BindShaderResource(player.setBinder);
        args.linear.vertexCount   = 3;
        graphicsEncoder.DrawLinear(args.linear);
        graphicsEncoder.EndPass();

        commandBuffer->End();

        vk::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
            std::pair<VkPipelineStageFlags, vk::SemaphorePtr>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable});

        commandBuffer->Submit(*graphicsQueue, submitInfo);

        vk::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex                 = imageIndex;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(presentInfo);

        VulkanSampleBase::OnTick(delta);
    }

    void VulkanTerrainVTSample::OnStart()
    {
        VulkanSampleBase::OnStart();

        VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10},
        };
        vk::DescriptorSetPool::VkDescriptor poolInfo = {};
        poolInfo.maxSets = 10;
        poolInfo.num     = sizeof(sizes) / sizeof(VkDescriptorPoolSize);
        poolInfo.sizes   = sizes;
        setPool          = device->CreateDeviceObject<vk::DescriptorSetPool>(poolInfo);

        SetupTerrain();
        SetupDescriptorSet();
        SetupPlayer();

        vertexInput = vk::VertexInput::Builder().Begin().Build();

        {
            vk::GraphicsPipeline::Program program;
            vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/TerrainAtlas.vert.spv");
            fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/TerrainAtlas.frag.spv");
            program.shaders.emplace_back(vs);
            program.shaders.emplace_back(fs);

            vk::GraphicsPipeline::State state = {};
            state.raster.cullMode             = VK_CULL_MODE_NONE;
            state.blends.blendStates.emplace_back(vk::GraphicsPipeline::BlendState{});

            vk::GraphicsPipeline::VkDescriptor psoDesc = {};
            psoDesc.program                          = &program;
            psoDesc.state                            = &state;
            psoDesc.pipelineLayout                   = pipelineLayout;
            psoDesc.vertexInput                      = vertexInput;
            psoDesc.renderPass                       = renderPass;
            pso                                      = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);
        }

        {
            vk::GraphicsPipeline::Program program;
            playerVs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/TerrainPlayer.vert.spv");
            playerFs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/TerrainPlayer.frag.spv");
            program.shaders.emplace_back(playerVs);
            program.shaders.emplace_back(playerFs);

            vk::GraphicsPipeline::State state = {};
            state.raster.cullMode             = VK_CULL_MODE_NONE;
            state.blends.blendStates.emplace_back(vk::GraphicsPipeline::BlendState{});

            vk::GraphicsPipeline::VkDescriptor psoDesc = {};
            psoDesc.program                          = &program;
            psoDesc.state                            = &state;
            psoDesc.pipelineLayout                   = playerLayout;
            psoDesc.vertexInput                      = vertexInput;
            psoDesc.renderPass                       = renderPass;
            playerPso                                = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);
        }
    }

    void VulkanTerrainVTSample::OnStop()
    {
        device->WaitIdle();
        vertexInput  = nullptr;
        setPool      = nullptr;
        globalBuffer = nullptr;
        globalSet    = nullptr;

        pso            = nullptr;
        pipelineLayout = nullptr;
        fs             = nullptr;
        vs             = nullptr;

        playerLayout       = nullptr;
        playerPso          = nullptr;
        playerVs           = nullptr;
        playerFs           = nullptr;
        player.setBinder   = nullptr;
        player.localBuffer = nullptr;
        player.set         = nullptr;

        terrain.set         = nullptr;
        terrain.setBinder   = nullptr;
        terrain.sampler     = nullptr;
        terrain.atlas       = nullptr;
        terrain.quadBuffer  = nullptr;
        terrain.localBuffer = nullptr;
        terrain.localSet    = nullptr;
        VulkanSampleBase::OnStop();
    }

    void VulkanTerrainVTSample::SetupPlayer()
    {
        vk::DescriptorSetLayout::VkDescriptor globalLayout = {};
        globalLayout.bindings.emplace(
            0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT});

        vk::DescriptorSetLayout::VkDescriptor localLayout = {};
        localLayout.bindings.emplace(
            0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT});

        vk::PipelineLayout::VkDescriptor pipelineLayoutInfo = {};
        pipelineLayoutInfo.desLayouts.emplace_back(globalLayout);
        pipelineLayoutInfo.desLayouts.emplace_back(localLayout);

        playerLayout = device->CreateDeviceObject<vk::PipelineLayout>(pipelineLayoutInfo);

        vk::Buffer::VkDescriptor bufferDesc = {};
        bufferDesc.size        = sizeof(sample::PlayerLocalData);
        bufferDesc.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferDesc.memory      = VMA_MEMORY_USAGE_CPU_TO_GPU;
        player.localBuffer = device->CreateDeviceObject<vk::Buffer>(bufferDesc);

        player.set = playerLayout->Allocate(setPool, 1);
        player.set->CreateWriter()
            .Write(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, player.localBuffer, 0, bufferDesc.size, 0)
            .Update();

        player.setBinder = std::make_shared<vk::DescriptorSetBinder>();
        player.setBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        player.setBinder->SetPipelineLayout(playerLayout);
        player.setBinder->BindSet(0, globalSet);
        player.setBinder->BindSet(1, player.set);

        player.localData.data.x = 0;
        player.localData.data.y = -512;
    }

    void VulkanTerrainVTSample::SetupDescriptorSet()
    {
        vk::DescriptorSetLayout::VkDescriptor globalLayout = {};
        globalLayout.bindings.emplace(
            0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT});

        vk::DescriptorSetLayout::VkDescriptor inputLayout = {};
        inputLayout.bindings.emplace(
            0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        inputLayout.bindings.emplace(
            1, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});

        vk::DescriptorSetLayout::VkDescriptor localLayout = {};
        localLayout.bindings.emplace(
            0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT});

        vk::PipelineLayout::VkDescriptor pipelineLayoutInfo = {};
        pipelineLayoutInfo.desLayouts.emplace_back(globalLayout);
        pipelineLayoutInfo.desLayouts.emplace_back(inputLayout);
        pipelineLayoutInfo.desLayouts.emplace_back(localLayout);

        pipelineLayout = device->CreateDeviceObject<vk::PipelineLayout>(pipelineLayoutInfo);

        vk::Buffer::VkDescriptor bufferDesc = {};
        bufferDesc.size        = sizeof(Matrix4);
        bufferDesc.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferDesc.memory      = VMA_MEMORY_USAGE_CPU_TO_GPU;
        globalBuffer = device->CreateDeviceObject<vk::Buffer>(bufferDesc);
        auto *ptr = globalBuffer->Map();
        Matrix4 projectMatrix = Matrix4::Identity();
        float near = 0.f;
        float far = 10.f;
        projectMatrix[0][0] = 1.f / swapChain->GetExtent().width;
        projectMatrix[1][1] = 1.f / swapChain->GetExtent().height;
        projectMatrix[2][2] = -2.f / (far - near);
        projectMatrix[3][2] = (far + near) / (far - near);
        projectMatrix[3][3] = 1.f;
        memcpy(ptr, &projectMatrix, sizeof(Matrix4));
        globalBuffer->UnMap();

        bufferDesc.size        = sizeof(sample::TerrainLocalData);
        bufferDesc.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferDesc.memory      = VMA_MEMORY_USAGE_CPU_TO_GPU;
        terrain.localBuffer = device->CreateDeviceObject<vk::Buffer>(bufferDesc);
        terrain.localData.data.x = terrainWidth;
        terrain.localData.data.y = terrainHeight;
        terrain.localData.data.z = widthNum;
        terrain.localData.data.w = heightNum;

        ptr = terrain.localBuffer->Map();
        memcpy(ptr, &terrain.localData, sizeof(sample::TerrainLocalData));
        terrain.localBuffer->UnMap();

        bufferDesc.size        = widthNum * heightNum * sizeof(sample::TerrainQuadData);
        bufferDesc.usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferDesc.memory      = VMA_MEMORY_USAGE_CPU_TO_GPU;
        terrain.quadBuffer = device->CreateDeviceObject<vk::Buffer>(bufferDesc);
        terrain.quadData.resize(widthNum * heightNum);

        globalSet = pipelineLayout->Allocate(setPool, 0);
        globalSet->CreateWriter()
            .Write(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, globalBuffer, 0, globalBuffer->GetSize(), 0)
            .Update();

        terrain.set = pipelineLayout->Allocate(setPool, 1);
        terrain.set->CreateWriter()
            .Write(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, terrain.atlas->GetImageView(), terrain.sampler, 0)
            .Write(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, terrain.quadBuffer, 0, terrain.quadBuffer->GetSize(), 0)
            .Update();

        terrain.localSet = pipelineLayout->Allocate(setPool, 2);
        terrain.localSet->CreateWriter()
            .Write(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, terrain.localBuffer, 0, terrain.localBuffer->GetSize(), 0)
            .Update();

        terrain.setBinder = std::make_shared<vk::DescriptorSetBinder>();
        terrain.setBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        terrain.setBinder->SetPipelineLayout(pipelineLayout);
        terrain.setBinder->BindSet(0, globalSet);
        terrain.setBinder->BindSet(1, terrain.set);
        terrain.setBinder->BindSet(2, terrain.localSet);
    }

    void VulkanTerrainVTSample::SetupTerrain()
    {
        terrain.width = widthNum;
        terrain.height = heightNum;
        terrain.path.resize(widthNum * heightNum);
        terrain.pages.resize(widthNum * heightNum);

        for (uint32_t i = 0; i < widthNum; ++i) {
            for (uint32_t j = 0; j < heightNum; ++j) {
                std::stringstream ss;
                ss << "terrain_" << i << "_" << j << ".ktx";
                Visit(terrain.path, widthNum, i, j) = ss.str();
            }
        }

        vk::SparseImage::VkDescriptor desc = {};
        desc.imageType   = VK_IMAGE_TYPE_2D;
        desc.format      = VK_FORMAT_R16_UNORM;
        desc.extent      = {widthNum * blockWidth, heightNum * blockWidth, 1};
        desc.mipLevels   = 1;
        desc.arrayLayers = 1;
        desc.usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        desc.viewType    = VK_IMAGE_VIEW_TYPE_2D;
        terrain.atlas = device->CreateDeviceObject<vk::SparseImage>(desc);
        terrain.sampler = device->CreateDeviceObject<vk::Sampler>({});

#ifdef STATIC_BINDING
        std::vector<vk::BufferPtr> tmpBuffers;
        auto cmd = device->GetGraphicsQueue()->AllocateCommandBuffer({});
        cmd->Begin();
        cmd->ImageBarrier(terrain.atlas->GetImage(), {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                          {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT},
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        for (uint32_t i = 0; i < widthNum; ++i) {
            for (uint32_t j = 0; j < heightNum; ++j) {
                ktxTexture* texture;
                KTX_error_code result;
                ktx_size_t offset;
                ktx_uint8_t* image;
                ktx_uint32_t level, layer, faceSlice;
                result = ktxTexture_CreateFromNamedFile((ENGINE_ROOT + "/assets/" + Visit(terrain.path, widthNum, i, j)).c_str(),
                                                        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                                        &texture);

                uint32_t numLevels = texture->numLevels;
                uint32_t baseWidth = texture->baseWidth;
                ktx_bool_t isArray = texture->isArray;

                level = 0; layer = 0; faceSlice = 0;
                result = ktxTexture_GetImageOffset(texture, level, layer, faceSlice, &offset);
                image = ktxTexture_GetData(texture) + offset;

                vk::Buffer::VkDescriptor bufferInfo = {};
                bufferInfo.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                bufferInfo.memory = VMA_MEMORY_USAGE_CPU_ONLY;
                bufferInfo.size = ktxTexture_GetImageSize(texture, 0);
                auto staging      = device->CreateDeviceObject<vk::Buffer>(bufferInfo);
                tmpBuffers.emplace_back(staging);
                uint8_t *dst = staging->Map();
                memcpy(dst, image, bufferInfo.size);
                staging->UnMap();
                ktxTexture_Destroy(texture);

                auto *page = terrain.atlas->AddPage({{(int)(blockWidth * i), (int)(blockHeight * j)}, {blockWidth, blockHeight, 1}});
                terrain.atlas->UpdateBinding();

                VkBufferImageCopy copyInfo = {};
                copyInfo.bufferOffset      = 0;
                copyInfo.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copyInfo.imageOffset       = page->offset;
                copyInfo.imageExtent       = page->extent;
                cmd->Copy(staging, terrain.atlas->GetImage(), copyInfo);
            }
        }

        cmd->ImageBarrier(terrain.atlas->GetImage(), {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                          {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT},
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        cmd->End();
        cmd->Submit(*graphicsQueue, {});
        cmd->Wait();
#endif
    }

    void VulkanTerrainVTSample::UpdateTerrainData()
    {
        uint8_t *ptr = terrain.quadBuffer->Map();
        memcpy(ptr, terrain.quadData.data(), terrain.quadData.size() * sizeof(sample::TerrainQuadData));
        terrain.quadBuffer->UnMap();
    }

    void VulkanTerrainVTSample::UpdateBinding()
    {
#ifndef STATIC_BINDING
        std::vector<ktxTexture *>            textures;
        std::vector<rhi::ImageUploadRequest> uploadRequest;

        uint32_t tWidth  = terrainWidth / widthNum;
        uint32_t tHeight = terrainHeight / heightNum;
        for (uint32_t i = 0; i < widthNum; ++i) {
            for (uint32_t j = 0; j < heightNum; ++j) {
                auto &quad = Visit(terrain.quadData, widthNum, i, j);

                float x = (i + 0.5f) * tWidth;
                float y = (j + 0.5f) * tHeight;

                float px = player.localData.data.x + (terrainWidth / 2.f) - x;
                float py = (terrainHeight / 2.f) - player.localData.data.y + -y;

                float    dist  = sqrt(px * px + py * py) / terrainWidth;
                uint32_t dstLevel = dist < 0.1f ? 0 : (dist < 0.3f ? 1 : 2);
                quad.level = dstLevel;
                auto &pageInfo = Visit(terrain.pages, widthNum, i, j);

                dstLevel = dist < 0.3f ? 0 : 2;
                if (pageInfo.level != dstLevel) {
                    if (pageInfo.page != nullptr) {
                        terrain.atlas->RemovePage(pageInfo.page, dstLevel == 2);
                        pageInfo.page = nullptr;
                    }
                    pageInfo.level = dstLevel;

                    if (dstLevel <= 1) {
                        pageInfo.page = terrain.atlas->AddPage({{(int)(blockWidth * i), (int)(blockHeight * j), 0}, {blockWidth, blockHeight, 1}});

                        ktxTexture* texture;
                        ktxTexture_CreateFromNamedFile((ENGINE_ROOT + "/assets/" + Visit(terrain.path, widthNum, i, j)).c_str(),
                                                                KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                                                &texture);
                        ktx_size_t offset = 0;
                        ktxTexture_GetImageOffset(texture, 0, 0, 0, &offset);
                        ktx_uint8_t* image = ktxTexture_GetData(texture) + offset;
                        textures.emplace_back(texture);

                        uploadRequest.emplace_back(rhi::ImageUploadRequest {
                            image, 0, ktxTexture_GetImageSize(texture, 0), 0, 0,
                            {
                                pageInfo.page->offset.x, pageInfo.page->offset.y, pageInfo.page->offset.z
                            },
                            {
                                pageInfo.page->extent.width, pageInfo.page->extent.height, pageInfo.page->extent.depth
                            }
                        });
                    }
                }
            }
        }
        terrain.atlas->UpdateBinding();

        auto uploadQueue = device->GetAsyncTransferQueue();
        auto handle = uploadQueue->UploadImage(terrain.atlas->GetImage(), uploadRequest);
        uploadQueue->Wait(handle);

        for (auto tex : textures) {
            ktxTexture_Destroy(tex);
        }
#endif
    }

    void VulkanTerrainVTSample::PlayerUpdate(float delta)
    {
        auto angle = 90.f / 180.f * 3.14f - player.localData.data.z;
        float dx = cos(angle) * 100 * delta;
        float dy = sin(angle) * 100 * delta;

        if (keys[KeyButton::KEY_UP]) {
            player.localData.data.x -= dx;
            player.localData.data.y += dy;
        }
        if (keys[KeyButton::KEY_LEFT]) {
            player.localData.data.z += delta * 50 / 180.f * 3.14f;
        }
        if (keys[KeyButton::KEY_RIGHT]) {
            player.localData.data.z -= delta * 50 / 180.f * 3.14f;
        }

        auto *ptr = player.localBuffer->Map();
        memcpy(ptr, &player.localData, sizeof(sample::PlayerLocalData));
        player.localBuffer->UnMap();
    }

    void VulkanTerrainVTSample::OnKeyUp(KeyButtonType button)
    {
        keys[button] = false;
    }

    void VulkanTerrainVTSample::OnKeyDown(KeyButtonType button)
    {
        keys[button] = true;
    }
}