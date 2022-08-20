//
// Created by Zach Lee on 2022/8/19.
//

#include <render/imgui/GuiRenderer.h>
#include <render/RenderConstants.h>
#include <imgui.h>

namespace sky {

    void GuiRenderer::Init()
    {
        shaders = std::make_shared<GraphicsShaderTable>();
        shaders->LoadShader("shaders/Gui.vert.spv", "Shaders/Gui.frag.spv");

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
        setBinder = technique->CreateSetBinder();
        assembly = std::make_shared<drv::VertexAssembly>();

        pool.reset(DescriptorPool::CreatePool(shaders->GetPipelineLayout()->GetLayout(0), {1}));
        set = pool->Allocate();

        CheckBufferSize(128 * sizeof(ImDrawVert), 128 * sizeof(ImDrawIdx));
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
            assembly->ResetVertexBuffer();
            assembly->AddVertexBuffer(vertexBuffer->GetRHIBuffer());
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
            assembly->SetIndexType(VK_INDEX_TYPE_UINT16);
            assembly->SetIndexBuffer(indexBuffer->GetRHIBuffer());
        }
    }

    void GuiRenderer::OnTick(float time)
    {
//        ImGui::NewFrame();
//        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
//        ImGui::End();
//
//        ImGui::Render();
    }

    void GuiRenderer::GatherRenderPrimitives()
    {

    }

    void GuiRenderer::OnRender()
    {
        ImDrawData* drawData = ImGui::GetDrawData();

        if (drawData == nullptr || drawData->TotalVtxCount == 0) {
            return;
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

}