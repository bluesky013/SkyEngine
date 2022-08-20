//
// Created by Zach Lee on 2022/8/19.
//

#include <render/imgui/GuiRenderer.h>
#include <render/RenderConstants.h>
#include <render/RenderViewport.h>
#include <render/imgui/GuiManager.h>
#include <imgui.h>

namespace sky {

    GuiRenderer::~GuiRenderer()
    {
        Event<IWindowEvent>::DisConnect(this);
    }

    void GuiRenderer::Init()
    {
        primitive = std::make_unique<GuiPrimitive>();

        shaders = std::make_shared<GraphicsShaderTable>();
        shaders->LoadShader("shaders/Gui.vert.spv", "Shaders/Gui.frag.spv");
        shaders->InitRHI();

        drv::VertexInput::Builder builder;
        auto vi = builder.Begin()
            .AddAttribute(0, 0, offsetof(ImDrawVert, pos), VK_FORMAT_R32G32_SFLOAT)
            .AddAttribute(1, 0, offsetof(ImDrawVert, uv),  VK_FORMAT_R32G32_SFLOAT)
            .AddAttribute(2, 0, offsetof(ImDrawVert, col), VK_FORMAT_R8G8B8A8_UNORM)
            .AddStream(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX)
            .Build();

        // TODO
        auto pass = std::make_shared<Pass>();
        SubPassInfo subPassInfo = {};
        subPassInfo.colors.emplace_back(AttachmentInfo{VK_FORMAT_B8G8R8A8_UNORM, VK_SAMPLE_COUNT_4_BIT});
        subPassInfo.depthStencil = AttachmentInfo{VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_4_BIT};
        pass->AddSubPass(subPassInfo);
        pass->InitRHI();

        technique = std::make_shared<GraphicsTechnique>();
        technique->SetShaderTable(shaders);
        technique->SetRenderPass(pass);
        technique->SetViewTag(MAIN_CAMERA_TAG);
        technique->SetDrawTag(0x01);  // TODO
        technique->SetDepthTestEn(true);
        technique->SetDepthWriteEn(true);
        primitive->pso = technique->AcquirePso(vi);
        primitive->setBinder = technique->CreateSetBinder();

        pool.reset(DescriptorPool::CreatePool(shaders->GetPipelineLayout()->GetLayout(0), {1}));
        primitive->set = pool->Allocate();
        primitive->set->UpdateTexture(0, GuiManager::Get()->GetFontTexture());
        primitive->set->Update();

        primitive->assembly = std::make_shared<drv::VertexAssembly>();
        CheckBufferSize(128 * sizeof(ImDrawVert), 128 * sizeof(ImDrawIdx));

        inited = true;
    }

    void GuiRenderer::CheckBufferSize(uint64_t vertexSize, uint64_t indexSize)
    {
        if (vertexSize > currentVertexSize) {
            Buffer::Descriptor bufferDesc = {};
            bufferDesc.size = vertexSize;
            bufferDesc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bufferDesc.allocCPU = false;
            bufferDesc.keepMap = true;
            bufferDesc.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
            vertexBuffer = std::make_shared<Buffer>(bufferDesc);
            vertexBuffer->InitRHI();

            currentVertexSize = vertexSize;
            primitive->assembly->ResetVertexBuffer();
            primitive->assembly->AddVertexBuffer(vertexBuffer->GetRHIBuffer());
        }

        if (indexSize > currentIndexSize) {
            Buffer::Descriptor bufferDesc = {};
            bufferDesc.size = vertexSize;
            bufferDesc.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bufferDesc.allocCPU = false;
            bufferDesc.keepMap = true;
            bufferDesc.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
            indexBuffer = std::make_shared<Buffer>(bufferDesc);
            indexBuffer->InitRHI();

            currentIndexSize = indexSize;
            primitive->assembly->SetIndexType(VK_INDEX_TYPE_UINT16);
            primitive->assembly->SetIndexBuffer(indexBuffer->GetRHIBuffer());
        }
    }

    void GuiRenderer::OnTick(float time)
    {
        ImGui::NewFrame();

        auto context = ImGui::GetCurrentContext();
        for (auto& widget : widgets) {
            widget->OnRender(context);
        }

        ImGui::Render();
    }

    void GuiRenderer::GatherRenderPrimitives()
    {
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData == nullptr || drawData->TotalVtxCount == 0) {
            return;
        }
        if (!inited) {
            Init();
        }

        uint64_t vertexSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
        uint64_t indexSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);
        CheckBufferSize(vertexSize, indexSize);

        auto* vertexDst = reinterpret_cast<ImDrawVert*>(vertexBuffer->GetMappedAddress());
        auto* indexDst = reinterpret_cast<ImDrawIdx*>(indexBuffer->GetMappedAddress());

        for (int i = 0; i < drawData->CmdListsCount; ++i) {
            const ImDrawList* cmdList = drawData->CmdLists[i];
            memcpy(vertexDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(indexDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            vertexDst += cmdList->VtxBuffer.Size;
            indexDst += cmdList->IdxBuffer.Size;
        }

        ImVec2 clipOff = drawData->DisplayPos;
        ImVec2 clipScale = drawData->FramebufferScale;

        uint32_t vtxOffset = 0;
        uint32_t idxOffset = 0;
        for (int i = 0; i < drawData->CmdListsCount; i++)
        {
            const ImDrawList* cmdList = drawData->CmdLists[i];
            for (int j = 0; j < cmdList->CmdBuffer.Size; j++)
            {
                const ImDrawCmd* pcmd = &cmdList->CmdBuffer[j];
            }
            idxOffset += cmdList->IdxBuffer.Size;
            vtxOffset += cmdList->VtxBuffer.Size;
        }
    }

    void GuiRenderer::OnBindViewport(const RenderViewport& viewport)
    {
        Event<IWindowEvent>::Connect(viewport.GetNativeHandle(), this);
    }

    void GuiRenderer::OnViewportSizeChange(const RenderViewport& viewport)
    {
        auto& ext = viewport.GetExtent();
        width = ext.width;
        height = ext.height;
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)width, (float)height);
    }
}