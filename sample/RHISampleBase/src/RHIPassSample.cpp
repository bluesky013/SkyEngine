//
// Created by Zach Lee on 2023/4/7.
//


#include <RHIPassSample.h>
#include <core/math/Vector4.h>
#include <core/math/Color.h>
#include <rhi/Buffer.h>
#include <rhi/Queue.h>
#include <builder/shader/ShaderCompiler.h>

namespace sky::rhi {

    struct MaterialData {
        Color baseColor;
    };

    struct MeshVertex {
        Vector4 pos;
        Vector4 normal;
    };

    template <typename T>
    struct VectorProvider : public BufferProvider {
        const uint8_t* GetData(uint64_t offset) const
        {
            return reinterpret_cast<const uint8_t *>(data.data()) + offset;
        }

        std::vector<T> data;
    };

    void RHIPassSample::SetupMaterial()
    {
        DescriptorSetLayout::Descriptor layoutDesc = {};
        layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{
            DescriptorType::UNIFORM_BUFFER, 1, 0, ShaderStageFlagBit::FS, "Constant"
        });
        auto layout = device->CreateDescriptorSetLayout(layoutDesc);
        material = std::make_shared<Material>();
        material->SetLayout(layout, sizeof(MaterialData));
        material->AddConnection("baseColor", {0, 0});
        material->SetValue("baseColor", Color{1.f, 1.f, 1.f, 1.f});
        material->Update();
    }

    void RHIPassSample::SetupMesh()
    {
        VertexInput::Descriptor viDesc = {};
        viDesc.attributes.emplace_back(VertexAttributeDesc{0, 0, 0, Format::F_RGBA32});
        viDesc.attributes.emplace_back(VertexAttributeDesc{1, 0, sizeof(Vector4), Format::F_RGBA32});
        viDesc.bindings.emplace_back(VertexBindingDesc{0, sizeof(MeshVertex)});
        auto vertexInput = device->CreateVertexInput(viDesc);

        std::vector<MeshVertex> vertices = {
            {{-0.5f, -0.5f,  0.5f, 1.f}, { 0.f,  0.f,  1.f, 1.f}},
            {{ 0.5f, -0.5f,  0.5f, 1.f}, { 0.f,  0.f,  1.f, 1.f}},
            {{ 0.5f,  0.5f,  0.5f, 1.f}, { 0.f,  0.f,  1.f, 1.f}},
            {{ 0.5f,  0.5f,  0.5f, 1.f}, { 0.f,  0.f,  1.f, 1.f}},
            {{-0.5f,  0.5f,  0.5f, 1.f}, { 0.f,  0.f,  1.f, 1.f}},
            {{-0.5f, -0.5f,  0.5f, 1.f}, { 0.f,  0.f,  1.f, 1.f}},

            {{ 0.5f, -0.5f,  0.5f, 1.f}, { 1.f,  0.f,  0.f, 1.f}},
            {{ 0.5f, -0.5f, -0.5f, 1.f}, { 1.f,  0.f,  0.f, 1.f}},
            {{ 0.5f,  0.5f, -0.5f, 1.f}, { 1.f,  0.f,  0.f, 1.f}},
            {{ 0.5f,  0.5f, -0.5f, 1.f}, { 1.f,  0.f,  0.f, 1.f}},
            {{ 0.5f,  0.5f,  0.5f, 1.f}, { 1.f,  0.f,  0.f, 1.f}},
            {{ 0.5f, -0.5f,  0.5f, 1.f}, { 1.f,  0.f,  0.f, 1.f}},

            {{ 0.5f, -0.5f, -0.5f, 1.f}, { 0.f,  0.f, -1.f, 1.f}},
            {{-0.5f, -0.5f, -0.5f, 1.f}, { 0.f,  0.f, -1.f, 1.f}},
            {{-0.5f,  0.5f, -0.5f, 1.f}, { 0.f,  0.f, -1.f, 1.f}},
            {{-0.5f,  0.5f, -0.5f, 1.f}, { 0.f,  0.f, -1.f, 1.f}},
            {{ 0.5f,  0.5f, -0.5f, 1.f}, { 0.f,  0.f, -1.f, 1.f}},
            {{ 0.5f, -0.5f, -0.5f, 1.f}, { 0.f,  0.f, -1.f, 1.f}},

            {{-0.5f, -0.5f, -0.5f, 1.f}, {-1.f,  0.f,  0.f, 1.f}},
            {{-0.5f, -0.5f,  0.5f, 1.f}, {-1.f,  0.f,  0.f, 1.f}},
            {{-0.5f,  0.5f,  0.5f, 1.f}, {-1.f,  0.f,  0.f, 1.f}},
            {{-0.5f,  0.5f,  0.5f, 1.f}, {-1.f,  0.f,  0.f, 1.f}},
            {{-0.5f,  0.5f, -0.5f, 1.f}, {-1.f,  0.f,  0.f, 1.f}},
            {{-0.5f, -0.5f, -0.5f, 1.f}, {-1.f,  0.f,  0.f, 1.f}},

            {{ 0.5f,  0.5f,  0.5f, 1.f}, { 0.f,  1.f,  0.f, 1.f}},
            {{ 0.5f,  0.5f, -0.5f, 1.f}, { 0.f,  1.f,  0.f, 1.f}},
            {{-0.5f,  0.5f, -0.5f, 1.f}, { 0.f,  1.f,  0.f, 1.f}},
            {{-0.5f,  0.5f, -0.5f, 1.f}, { 0.f,  1.f,  0.f, 1.f}},
            {{-0.5f,  0.5f,  0.5f, 1.f}, { 0.f,  1.f,  0.f, 1.f}},
            {{ 0.5f,  0.5f,  0.5f, 1.f}, { 0.f,  1.f,  0.f, 1.f}},

            {{ 0.5f, -0.5f,  0.5f, 1.f}, { 0.f, -1.f,  0.f, 1.f}},
            {{ 0.5f, -0.5f, -0.5f, 1.f}, { 0.f, -1.f,  0.f, 1.f}},
            {{-0.5f, -0.5f, -0.5f, 1.f}, { 0.f, -1.f,  0.f, 1.f}},
            {{-0.5f, -0.5f, -0.5f, 1.f}, { 0.f, -1.f,  0.f, 1.f}},
            {{-0.5f, -0.5f,  0.5f, 1.f}, { 0.f, -1.f,  0.f, 1.f}},
            {{ 0.5f, -0.5f,  0.5f, 1.f}, { 0.f, -1.f,  0.f, 1.f}},
        };
        std::vector<uint16_t> indices = {
             0,  1,  2,  3,  4,  5,
             6,  7,  8,  9, 10, 11,
            12, 13, 14, 15, 16, 17,
            18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29,
            30, 31, 32, 33, 34, 35
        };

        uint64_t vertexSize = vertices.size() * sizeof(MeshVertex);
        uint64_t indexSize = indices.size() * sizeof(uint16_t);
        auto *queue = device->GetQueue(QueueType::TRANSFER);


        Buffer::Descriptor bufferDesc = {};
        bufferDesc.size   = vertexSize;
        bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST | BufferUsageFlagBit::VERTEX;
        bufferDesc.memory = MemoryType::GPU_ONLY;
        auto vb = device->CreateBuffer(bufferDesc);
        auto vp = std::make_shared<VectorProvider<MeshVertex>>();
        vp->data.swap(vertices);
        auto vh = queue->UploadBuffer(vb, {vp, 0, vertexSize});

        bufferDesc.size = indexSize;
        bufferDesc.usage = BufferUsageFlagBit::TRANSFER_DST | BufferUsageFlagBit::INDEX;
        auto ib = device->CreateBuffer(bufferDesc);
        auto ip = std::make_shared<VectorProvider<uint16_t>>();
        ip->data.swap(indices);
        auto ih = queue->UploadBuffer(ib, {ip, 0, indexSize});

        queue->Wait(vh);
        queue->Wait(ih);

        VertexAssembly::Descriptor vaDesc;
        vaDesc.vertexInput = vertexInput;
        vaDesc.vertexBuffers.emplace_back(vb->CreateView({0, vertexSize}));
        vaDesc.indexBuffer = ib->CreateView({0, indexSize});
        auto va = device->CreateVertexAssembly(vaDesc);
        auto localSet = device->CreateDescriptorSet({localLayout});

        PipelineLayout::Descriptor ppLayoutDesc = {{
            globalLayout,
            material->GetLayout(),
            localLayout,
        }};
        auto pipelineLayout = device->CreatePipelineLayout(ppLayoutDesc);

        builder::ShaderCompiler::CompileShader("pos_normal_vs.glsl", {"shaders/RHISample/pos_normal_vs.shader", builder::ShaderType::VS});
        builder::ShaderCompiler::CompileShader("base_color_fs.glsl", {"shaders/RHISample/base_color_fs.shader", builder::ShaderType::FS});

        gfxTech = std::make_shared<GraphicsTechnique>();
        gfxTech->SetShader(CreateShader(rhi, *device, ShaderStageFlagBit::VS, "shaders/RHISample/pos_normal_vs.shader"),
                           CreateShader(rhi, *device, ShaderStageFlagBit::FS, "shaders/RHISample/base_color_fs.shader"));
        gfxTech->SetVertexInput(vertexInput);
        gfxTech->SetPipelineLayout(pipelineLayout);
        gfxTech->SetRenderPass(renderPass);
        gfxTech->psoDesc.state.depthStencil.depthTest = true;
        gfxTech->psoDesc.state.depthStencil.depthWrite = true;
        gfxTech->psoDesc.state.blendStates.emplace_back(BlendState{});
        gfxTech->BuildPso();

        mesh = std::make_shared<Mesh>();
        mesh->SetVA(va);
        mesh->AddSubMesh({0, 36, material, gfxTech});
        mesh->SetLocalSet(localSet);
    }

    void RHIPassSample::SetupCamera()
    {
        auto &ext = swapChain->GetExtent();
        camera = std::make_shared<Camera>();
        camera->MakeProjective(60.f / 180.f * 3.14f, ext.width / static_cast<float>(ext.height), 0.1f, 100.f);

        auto matrix = Matrix4::Identity();
        matrix[3][0] = 0.f;
        matrix[3][1] = 1.0f;
        matrix[3][2] = 5.f;
        camera->SetTransform(matrix);
        camera->Update();

        Buffer::Descriptor bufferDesc = {};
        bufferDesc.size = sizeof(CameraData);
        bufferDesc.usage = BufferUsageFlagBit::UNIFORM;
        bufferDesc.memory = MemoryType::CPU_TO_GPU;
        cameraBuffer = device->CreateBuffer(bufferDesc);
        uint8_t *ptr = cameraBuffer->Map();
        memcpy(ptr, &camera->GetData(), sizeof(CameraData));
        cameraBuffer->UnMap();
    }

    void RHIPassSample::SetupLayout()
    {
        {
            DescriptorSetLayout::Descriptor layoutDesc = {};
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{
                DescriptorType::UNIFORM_BUFFER, 1, 0, ShaderStageFlagBit::VS | ShaderStageFlagBit::FS, "CameraData"
            });
            globalLayout = device->CreateDescriptorSetLayout(layoutDesc);
        }

        {
            DescriptorSetLayout::Descriptor layoutDesc = {};
            layoutDesc.bindings.emplace_back(DescriptorSetLayout::SetBinding{
                DescriptorType::UNIFORM_BUFFER, 1, 0, ShaderStageFlagBit::VS, "LocalData"
            });
            localLayout = device->CreateDescriptorSetLayout(layoutDesc);
        }

        {
            auto set = device->CreateDescriptorSet({globalLayout});
            set->BindBuffer(0, cameraBuffer->CreateView({0, sizeof(CameraData)}), 0);
            set->Update();

            scene = std::make_shared<Scene>();
            scene->SetGlobalSet(set);
        }

    }

    void RHIPassSample::SetupScene()
    {
        SetupCamera();
        SetupLayout();
        SetupPass();

        SetupMaterial();
        SetupMesh();
    }

    void RHIPassSample::SetupBase()
    {
        SetupPass();
        SetupTriangle();

        SetupScene();
    }

    void RHIPassSample::OnStop()
    {
        material = nullptr;
        camera   = nullptr;
        mesh     = nullptr;
        scene    = nullptr;

        gfxTech      = nullptr;
        globalLayout = nullptr;
        localLayout  = nullptr;
        cameraBuffer = nullptr;

        RHISampleBase::OnStop();
    }

    void RHIPassSample::OnTick(float delta)
    {
        auto queue = device->GetQueue(QueueType::GRAPHICS);
        uint32_t index = swapChain->AcquireNextImage(imageAvailable);

        SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
            std::pair<PipelineStageFlags , SemaphorePtr>{PipelineStageBit::COLOR_OUTPUT, imageAvailable});

        ImageBarrier barrier = {};
        barrier.srcStage = PipelineStageBit::TOP;
        barrier.dstStage = PipelineStageBit::COLOR_OUTPUT;
        barrier.srcFlag = AccessFlag::NONE;
        barrier.dstFlag = AccessFlag::COLOR_WRITE;
        barrier.srcQueueFamily = (~0U);
        barrier.dstQueueFamily = (~0U);
        barrier.mask      = rhi::AspectFlagBit::COLOR_BIT;
        barrier.subRange  = {0, 1, 0, 1};
        barrier.image = swapChain->GetImage(index);

        commandBuffer->Begin();

        commandBuffer->QueueBarrier(barrier);
        commandBuffer->FlushBarriers();

        auto encoder = commandBuffer->EncodeGraphics();
        encoder->BeginPass({frameBuffers[index], renderPass, 2, clears.data()});
        for (auto &subMesh : mesh->subMeshes) {
            encoder->BindPipeline(subMesh.tech->pso);
            encoder->BindSet(0, scene->GetGlobalSet());
            encoder->BindSet(1, subMesh.material->GetSet());
            encoder->BindSet(2, mesh->descriptorSet);
            encoder->BindAssembly(mesh->vao);
            encoder->DrawIndexed({subMesh.indexCount, 1, 0, 0, 0});
        }
        encoder->EndPass();

        barrier.srcStage = PipelineStageBit::COLOR_OUTPUT;
        barrier.dstStage = PipelineStageBit::BOTTOM;
        barrier.srcFlag = AccessFlag::COLOR_WRITE;
        barrier.dstFlag = AccessFlag::PRESENT;
        commandBuffer->QueueBarrier(barrier);
        commandBuffer->FlushBarriers();

        commandBuffer->End();
        commandBuffer->Submit(*queue, submitInfo);

        PresentInfo presentInfo = {};
        presentInfo.imageIndex = index;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(*queue, presentInfo);
    }
}