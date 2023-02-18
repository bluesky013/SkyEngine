//
// Created by Zach Lee on 2022/11/5.
//

#include "RHISampleBase.h"
#include <rhi/Queue.h>
#include <core/file/FileIO.h>
#include <builder/shader/ShaderCompiler.h>
#include <filesystem>

namespace sky::rhi {

    ShaderPtr CreateShader(API api, Device &device, ShaderStageFlagBit stage, const std::string &path)
    {
        Shader::Descriptor shaderDesc = {};
        shaderDesc.stage = stage;

        std::string shaderPath = path;
        if (api == API::VULKAN) {
            shaderPath += ".spv";
        } else if (api == API::METAL) {
            shaderPath += ".msl";
        } else if (api == API::GLES) {
            shaderPath += ".gles";
        }
        ReadBin(shaderPath, shaderDesc.data);
        return device.CreateShader(shaderDesc);
    }

    void RHISampleBase::OnStart()
    {
        builder::ShaderCompiler::CompileShader("fullscreen.glsl", {"shaders/rhiSample/fullscreen.shader", builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("descriptor_fs.glsl", {"shaders/rhiSample/descriptor_fs.shader", builder::ShaderType::FS});

        auto systemApi = Interface<ISystemNotify>::Get()->GetApi();
        instance = Instance::Create({"", "", false, rhi});
        device   = instance->CreateDevice({});

        auto nativeWindow = systemApi->GetViewport();
        Event<IWindowEvent>::Connect(nativeWindow->GetNativeHandle(), this);
        SwapChain::Descriptor swcDesc = {};

        swcDesc.window = nativeWindow->GetNativeHandle();
        swcDesc.width  = nativeWindow->GetWidth();
        swcDesc.height = nativeWindow->GetHeight();
        swapChain      = device->CreateSwapChain(swcDesc);

        auto format = swapChain->GetFormat();
        auto &ext = swapChain->GetExtent();
        uint32_t count = swapChain->GetImageCount();

        RenderPass::Descriptor passDesc = {};
        passDesc.attachments.emplace_back(RenderPass::Attachment{
            format,
            SampleCount::X1,
            LoadOp::CLEAR,
            StoreOp::STORE,
            LoadOp::DONT_CARE,
            StoreOp::DONT_CARE,
        });
        passDesc.subPasses.emplace_back(RenderPass::SubPass {
            {0}, {}, {}, ~(0U)
        });
        renderPass = device->CreateRenderPass(passDesc);

        FrameBuffer::Descriptor fbDesc = {};
        fbDesc.extent = ext;
        fbDesc.pass = renderPass;
        frameBuffers.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            fbDesc.views.emplace_back(swapChain->GetImage(i)->CreateView({}));
            frameBuffers.emplace_back(device->CreateFrameBuffer(fbDesc));
        }

        CommandBuffer::Descriptor cmdDesc = {};
        commandBuffer = device->CreateCommandBuffer(cmdDesc);

        GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.state;
        psoDesc.vs = CreateShader(rhi, *device, ShaderStageFlagBit::VS, "shaders/RHISample/fullscreen.shader");
        psoDesc.fs = CreateShader(rhi, *device, ShaderStageFlagBit::FS, "shaders/RHISample/descriptor_fs.shader");
        psoDesc.renderPass = renderPass;
        psoDesc.pipelineLayout;
        psoDesc.vertexInput;
        pso = device->CreateGraphicsPipeline(psoDesc);

        rhi::Image::Descriptor imageDesc = {};
        imageDesc.imageType   = ImageType::IMAGE_2D;
        imageDesc.format      = PixelFormat::RGBA8_UNORM;
        imageDesc.extent      = {4, 4, 1};
        imageDesc.usage       = ImageUsageFlagBit::TRANSFER_DST | ImageUsageFlagBit::SAMPLED;
        image = device->CreateImage(imageDesc);

        struct RGBA8 {
            union {
                uint8_t data[4];
                struct {
                    uint8_t r;
                    uint8_t g;
                    uint8_t b;
                    uint8_t a;
                };
            };
        };
        RGBA8 data[4][4];
        for (uint32_t i = 0; i < 4; ++i) {
            for (uint32_t j = 0; j < 4; ++j) {
                auto &dat = data[i][j];
                dat.r = i * (256 / 4);
                dat.g = j * (256 / 4);
                dat.b = 255;
                dat.a = 255;
            }
        }
        auto tq = device->GetQueue(QueueType::TRANSFER);
        rhi::ImageUploadRequest uploadRequest = {};
        uploadRequest.data     = reinterpret_cast<const uint8_t*>(data);
        uploadRequest.size     = 16 * sizeof(RGBA8);
        uploadRequest.imageOffset = {0, 0, 0};
        uploadRequest.imageExtent = {4, 4, 1};

        auto handle = tq->UploadImage(image, uploadRequest);
        tq->Wait(handle);
    }

    void RHISampleBase::OnStop()
    {
        commandBuffer = nullptr;
        pso = nullptr;
        image = nullptr;
        swapChain = nullptr;
        renderPass = nullptr;
        frameBuffers.clear();

        delete device;
        device = nullptr;

        Instance::Destroy(instance);
        instance = nullptr;
    }

    void RHISampleBase::OnTick(float delta)
    {
        ClearValue clear = {};
        clear.color.float32[0] = 1.f;
        clear.color.float32[1] = 0.f;
        clear.color.float32[2] = 0.f;
        clear.color.float32[3] = 1.f;

        auto queue = device->GetQueue(QueueType::GRAPHICS);
        uint32_t index = swapChain->AcquireNextImage();

        commandBuffer->Begin();
        commandBuffer->EncodeGraphics()->BeginPass(frameBuffers[index], renderPass, 1, &clear)
            .BindPipeline(pso)
            .DrawLinear({3, 1, 0, 0})
            .EndPass();
        commandBuffer->End();
        commandBuffer->Submit(*queue);
    }

    void RHISampleBase::OnWindowResize(uint32_t width, uint32_t height)
    {

    }
}
