//
// Created by Zach Lee on 2022/8/19.
//

#include <imgui.h>
#include <render/RenderConstants.h>
#include <render/RenderViewport.h>
#include <render/imgui/GuiManager.h>
#include <render/imgui/GuiRenderer.h>

namespace sky {

    struct GuiConstants {
        ImVec2 scale;
        ImVec2 translate;
    };

    GuiRenderer::~GuiRenderer()
    {
        Event<IWindowEvent>::DisConnect(this);
    }

    void GuiRenderer::Init()
    {
        primitive = std::make_unique<GuiPrimitive>();
        primitive->SetViewTag(MAIN_CAMERA_TAG);
        primitive->SetDrawTag(UI_TAG);

        shaders = std::make_shared<GraphicsShaderTable>();
        shaders->LoadShader("shaders/Gui.vert.spv", "Shaders/Gui.frag.spv");
        shaders->InitRHI();

        drv::VertexInput::Builder builder;
        auto                      vi = builder.Begin()
                      .AddAttribute(0, 0, offsetof(ImDrawVert, pos), VK_FORMAT_R32G32_SFLOAT)
                      .AddAttribute(1, 0, offsetof(ImDrawVert, uv), VK_FORMAT_R32G32_SFLOAT)
                      .AddAttribute(2, 0, offsetof(ImDrawVert, col), VK_FORMAT_R8G8B8A8_UNORM)
                      .AddStream(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX)
                      .Build();

        // TODO
        auto        pass        = std::make_shared<Pass>();
        SubPassInfo subPassInfo = {};
        subPassInfo.colors.emplace_back(AttachmentInfo{VK_FORMAT_B8G8R8A8_UNORM, VK_SAMPLE_COUNT_1_BIT});
        pass->AddSubPass(subPassInfo);
        pass->InitRHI();

        technique = std::make_shared<GraphicsTechnique>();
        technique->SetShaderTable(shaders);
        technique->SetRenderPass(pass);
        technique->SetViewTag(MAIN_CAMERA_TAG);
        technique->SetDrawTag(0x01); // TODO
        technique->SetDepthTestEn(true);
        technique->SetDepthWriteEn(true);
        auto &pipelineState                                     = technique->GetState();
        pipelineState.blends.attachments[0].blendEnable         = VK_TRUE;
        pipelineState.blends.attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        pipelineState.blends.attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        pipelineState.blends.attachments[0].colorBlendOp        = VK_BLEND_OP_ADD;
        pipelineState.blends.attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineState.blends.attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        pipelineState.blends.attachments[0].alphaBlendOp        = VK_BLEND_OP_ADD;
        pipelineState.blends.attachments[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        primitive->pso       = technique->AcquirePso(vi);
        primitive->setBinder = technique->CreateSetBinder();
        primitive->constants = drv::PushConstants::CreateFromPipelineLayout(shaders->GetPipelineLayout());

        pool.reset(DescriptorPool::CreatePool(shaders->GetPipelineLayout()->GetLayout(0), {1}));
        primitive->set = pool->Allocate();
        primitive->set->UpdateTexture(0, GuiManager::Get()->GetFontTexture());
        primitive->set->Update();

        primitive->setBinder->BindSet(0, primitive->set->GetRHISet());

        primitive->assembly = std::make_shared<drv::VertexAssembly>();

        inited = true;
    }

    void GuiRenderer::CheckBufferSize(uint64_t vertexSize, uint64_t indexSize)
    {
        if (vertexSize > currentVertexSize) {
            Buffer::Descriptor bufferDesc = {};
            bufferDesc.size               = vertexSize;
            bufferDesc.usage              = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bufferDesc.allocCPU           = false;
            bufferDesc.keepMap            = true;
            bufferDesc.memory             = VMA_MEMORY_USAGE_CPU_TO_GPU;
            vertexBuffer                  = std::make_shared<Buffer>(bufferDesc);
            vertexBuffer->InitRHI();

            currentVertexSize = vertexSize;
            primitive->assembly->ResetVertexBuffer();
            primitive->assembly->AddVertexBuffer(vertexBuffer->GetRHIBuffer());
        }

        if (indexSize > currentIndexSize) {
            Buffer::Descriptor bufferDesc = {};
            bufferDesc.size               = vertexSize;
            bufferDesc.usage              = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bufferDesc.allocCPU           = false;
            bufferDesc.keepMap            = true;
            bufferDesc.memory             = VMA_MEMORY_USAGE_CPU_TO_GPU;
            indexBuffer                   = std::make_shared<Buffer>(bufferDesc);
            indexBuffer->InitRHI();

            currentIndexSize = indexSize;
            primitive->assembly->SetIndexType(VK_INDEX_TYPE_UINT16);
            primitive->assembly->SetIndexBuffer(indexBuffer->GetRHIBuffer());
        }
    }

    void GuiRenderer::OnTick(float time)
    {

        ImGuiIO &io  = ImGui::GetIO();
        io.DeltaTime = time;

        ImGui::NewFrame();
        auto context = ImGui::GetCurrentContext();
        for (auto &widget : widgets) {
            widget->OnRender(context);
        }

        ImGui::Render();
    }

    void GuiRenderer::GatherRenderPrimitives()
    {
        ImDrawData *drawData = ImGui::GetDrawData();
        if (drawData == nullptr || drawData->TotalVtxCount == 0) {
            return;
        }
        if (!inited) {
            Init();
        }

        uint64_t vertexSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
        uint64_t indexSize  = drawData->TotalIdxCount * sizeof(ImDrawIdx);
        CheckBufferSize(vertexSize, indexSize);

        auto *vertexDst = reinterpret_cast<ImDrawVert *>(vertexBuffer->GetMappedAddress());
        auto *indexDst  = reinterpret_cast<ImDrawIdx *>(indexBuffer->GetMappedAddress());

        for (int i = 0; i < drawData->CmdListsCount; ++i) {
            const ImDrawList *cmdList = drawData->CmdLists[i];
            memcpy(vertexDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(indexDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            vertexDst += cmdList->VtxBuffer.Size;
            indexDst += cmdList->IdxBuffer.Size;
        }

        ImVec2 clipOff   = drawData->DisplayPos;
        ImVec2 clipScale = drawData->FramebufferScale;

        GuiConstants currentConstant;
        currentConstant.scale.x     = 2.0f / drawData->DisplaySize.x;
        currentConstant.scale.y     = 2.0f / drawData->DisplaySize.y;
        currentConstant.translate.x = -1.0f - drawData->DisplayPos.x * currentConstant.scale[0];
        currentConstant.translate.y = -1.0f - drawData->DisplayPos.y * currentConstant.scale[1];
        primitive->constants->WriteData(currentConstant, 0);
        primitive->dc.clear();

        uint32_t vtxOffset = 0;
        uint32_t idxOffset = 0;
        for (int i = 0; i < drawData->CmdListsCount; i++) {
            const ImDrawList *cmdList = drawData->CmdLists[i];
            for (int j = 0; j < cmdList->CmdBuffer.Size; j++) {
                const ImDrawCmd *pcmd = &cmdList->CmdBuffer[j];

                ImVec2 clipMin((pcmd->ClipRect.x - clipOff.x) * clipScale.x, (pcmd->ClipRect.y - clipOff.y) * clipScale.y);
                ImVec2 clipMax((pcmd->ClipRect.z - clipOff.x) * clipScale.x, (pcmd->ClipRect.w - clipOff.y) * clipScale.y);

                if (clipMin.x < 0.0f) {
                    clipMin.x = 0.0f;
                }
                if (clipMin.y < 0.0f) {
                    clipMin.y = 0.0f;
                }
                if (clipMax.x > width) {
                    clipMax.x = (float)width;
                }
                if (clipMax.y > height) {
                    clipMax.y = (float)height;
                }
                if (clipMax.x < clipMin.x || clipMax.y < clipMin.y)
                    continue;

                GuiPrimitive::DrawCall drawCall{};
                drawCall.scissor.offset.x      = (int32_t)(clipMin.x);
                drawCall.scissor.offset.y      = (int32_t)(clipMin.y);
                drawCall.scissor.extent.width  = (uint32_t)(clipMax.x - clipMin.x);
                drawCall.scissor.extent.height = (uint32_t)(clipMax.y - clipMin.y);

                drawCall.indexed.indexCount   = pcmd->ElemCount;
                drawCall.indexed.firstIndex   = pcmd->IdxOffset + idxOffset;
                drawCall.indexed.vertexOffset = pcmd->VtxOffset + vtxOffset;
                primitive->dc.emplace_back(drawCall);
            }
            idxOffset += cmdList->IdxBuffer.Size;
            vtxOffset += cmdList->VtxBuffer.Size;
        }

        auto &views = scene.GetViews();
        for (auto &view : views) {
            view->AddRenderPrimitive(primitive.get());
        }
    }

    void GuiRenderer::OnBindViewport(const RenderViewport &viewport)
    {
        Event<IWindowEvent>::Connect(viewport.GetNativeHandle(), this);
    }

    void GuiRenderer::OnViewportSizeChange(const RenderViewport &viewport)
    {
        auto &ext      = viewport.GetExtent();
        width          = ext.width;
        height         = ext.height;
        ImGuiIO &io    = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)width, (float)height);
    }

    void GuiRenderer::OnMouseMove(int32_t x, int32_t y)
    {
        ImGuiIO &io   = ImGui::GetIO();
        io.MousePos.x = static_cast<float>(x);
        io.MousePos.y = static_cast<float>(y);
    }

    void GuiRenderer::OnMouseButtonDown(MouseButtonType button)
    {
        ImGuiIO &io              = ImGui::GetIO();
        io.MouseDown[button - 1] = true;
    }

    void GuiRenderer::OnMouseButtonUp(MouseButtonType button)
    {
        ImGuiIO &io              = ImGui::GetIO();
        io.MouseDown[button - 1] = false;
    }

    void GuiRenderer::OnMouseWheel(int32_t wheelX, int32_t wheelY)
    {
        ImGuiIO &io = ImGui::GetIO();
        if (wheelX > 0)
            io.MouseWheelH += 1;
        if (wheelX < 0)
            io.MouseWheelH -= 1;
        if (wheelY > 0)
            io.MouseWheel += 1;
        if (wheelY < 0)
            io.MouseWheel -= 1;
    }

    void GuiRenderer::OnKeyUp(KeyButtonType key)
    {
        ImGuiIO &io      = ImGui::GetIO();
        io.KeysDown[key] = false;
    }

    void GuiRenderer::OnKeyDown(KeyButtonType key)
    {
        ImGuiIO &io      = ImGui::GetIO();
        io.KeysDown[key] = true;
    }

    void GuiRenderer::OnTextInput(const char *text)
    {
        ImGuiIO &io = ImGui::GetIO();
        io.AddInputCharactersUTF8(text);
    }
} // namespace sky