//
// Created by Zach Lee on 2022/11/5.
//

#include "RHISampleBase.h"
#include <rhi/Queue.h>

namespace sky::rhi {

    const char* VS = "#version 320 es\n"
                     "layout(location = 0) out vec2 vUv;"
                     "vec2 positions[3] = vec2[]("
                     "    vec2(-1.0,  3.0),"
                     "    vec2(-1.0, -1.0),"
                     "    vec2( 3.0, -1.0)"
                     ");"
                     "vec2 uv[3] = vec2[]("
                     "    vec2(0.0, 2.0),"
                     "    vec2(0.0, 0.0),"
                     "    vec2(2.0, 0.0)"
                     ");"
                     "void main()"
                     "{"
                     "    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);"
                     "    vUv = uv[gl_VertexID];"
                     "}";

    const char* FS = "#version 320 es\n"
                     "precision highp float;"
                     "layout(location = 0) in vec2 vUv;"
                     "layout(location = 0) out vec4 outColor;"
                     "void main()"
                     "{"
                     "    outColor = vec4(0, 0, 1, 1);"
                     "}";

    ShaderPtr CreateShader(Device &device, ShaderStageFlagBit stage, const char* source)
    {
        Shader::Descriptor shaderDesc = {};
        shaderDesc.stage = stage;
        shaderDesc.data.resize(strlen(source) + 1);
        memcpy(shaderDesc.data.data(), source, strlen(source) + 1);
        return device.CreateShader(shaderDesc);
    }

    void RHISampleBase::OnStart()
    {
        instance = Instance::Create({"", "", false, API::GLES});
        device   = instance->CreateDevice({});

        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
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
        psoDesc.vs = CreateShader(*device, ShaderStageFlagBit::VS, VS);
        psoDesc.fs = CreateShader(*device, ShaderStageFlagBit::FS, FS);
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
        swapChain = nullptr;

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
