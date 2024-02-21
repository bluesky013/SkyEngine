//
// Created by blues on 2023/9/20.
//

#include <imgui/ImGuiInstance.h>
#include <framework/window/NativeWindow.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <render/RenderScene.h>
#include <imgui/ImGuiFeature.h>
#include <render/rdg/RenderGraph.h>

namespace sky {
    struct UITransform {
        ImVec2 scale;
        ImVec2 translate;
    };

    ImGuiInstance::ImGuiInstance()
    {
        IMGUI_CHECKVERSION();
        context = ImGui::CreateContext();
        ImGui::SetCurrentContext(context);
        ImGuiIO& io = ImGui::GetIO();

        unsigned char *pixels;
        int width;
        int height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        uint32_t uploadSize = width * height * 4 * sizeof(char);

        auto *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
        fontTexture = std::make_shared<Texture2D>();
        fontTexture->Init(rhi::PixelFormat::RGBA8_UNORM, width, height, 1);
        uploadHandle = fontTexture->Upload(pixels, uploadSize, *queue);

        ubo = std::make_shared<DynamicUniformBuffer>();
        ubo->Init(sizeof(UITransform), Renderer::Get()->GetInflightFrameCount());

        io.Fonts->SetTexID((ImTextureID)(intptr_t)fontTexture->GetImageView().get());

        auto *feature = ImGuiFeature::Get();
        primitive = std::make_unique<RenderPrimitive>();
        primitive->techniques.emplace_back(feature->GetDefaultTech());

        queue->Wait(uploadHandle);
        globalSet = feature->RequestResourceGroup();
        globalSet->BindTexture("FontTexture", fontTexture->GetImageView(), 0);
        globalSet->BindDynamicUBO("Constants", ubo, 0);
        globalSet->Update();

        primitive->batchSet = globalSet;
    }

    ImGuiInstance::~ImGuiInstance()
    {
        ImGui::DestroyContext(context);
        Event<IWindowEvent>::DisConnect( this);
    }

    void ImGuiInstance::Tick(float delta)
    {
        MakeCurrent();
        ImGuiIO& io = ImGui::GetIO();
        io.DeltaTime = delta;

        ImGui::NewFrame();

        for (auto &widget : widgets) {
            widget->Execute(context);
        }

        ImGui::EndFrame();

        ImGui::Render();
        drawData = ImGui::GetDrawData();
    }

    void ImGuiInstance::Render(rdg::RenderGraph &rdg)
    {
        UITransform transform;
        transform.scale.x = 2.0f / drawData->DisplaySize.x;
        transform.scale.y = 2.0f / drawData->DisplaySize.y;
        transform.translate.x = -1.0f - drawData->DisplayPos.x * transform.scale.x;
        transform.translate.y = -1.0f - drawData->DisplayPos.y * transform.scale.y;
        ubo->Write(0, transform);
        ubo->Upload();

        float fbWidth  = drawData->DisplaySize.x * drawData->FramebufferScale.x;
        float fbHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;

        ImVec2 clipOff   = drawData->DisplayPos;
        ImVec2 clipScale = drawData->FramebufferScale;

        int globalVtxOffset = 0;
        int globalIdxOffset = 0;

        if (drawData->TotalIdxCount == 0 || drawData->TotalVtxCount == 0) {
            return;
        }

        RenderCmdList package;
        CheckVertexBuffers(drawData->TotalVtxCount, drawData->TotalIdxCount);
        auto *vtxDst = reinterpret_cast<ImDrawVert *>(stagingBuffer->GetRHIBuffer()->Map());
        auto *idxDst = reinterpret_cast<ImDrawIdx *>(stagingBuffer->GetRHIBuffer()->Map() + vertexSize);

        primitive->args.clear();
        for (int n = 0; n < drawData->CmdListsCount; n++) {
            const ImDrawList *cmdList = drawData->CmdLists[n];
            // upload vertex buffers
            memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmdList->VtxBuffer.Size;
            idxDst += cmdList->IdxBuffer.Size;

            for (int i = 0; i < cmdList->CmdBuffer.Size; i++) {
                const ImDrawCmd *pCmd = &cmdList->CmdBuffer[i];
                ImVec2 clipMin((pCmd->ClipRect.x - clipOff.x) * clipScale.x, (pCmd->ClipRect.y - clipOff.y) * clipScale.y);
                ImVec2 clipMax((pCmd->ClipRect.z - clipOff.x) * clipScale.x, (pCmd->ClipRect.w - clipOff.y) * clipScale.y);

                if (clipMin.x < 0.0f) { clipMin.x = 0.0f; }
                if (clipMin.y < 0.0f) { clipMin.y = 0.0f; }
                if (clipMax.x > fbWidth) { clipMax.x = fbWidth; }
                if (clipMax.y > fbHeight) { clipMax.y = fbHeight; }
                if (clipMax.x < clipMin.x || clipMax.y < clipMin.y) { continue; }

                primitive->args.emplace_back(rhi::Rect2D{
                    {static_cast<int32_t>((clipMin.x)), static_cast<int32_t>((clipMin.y))},
                    {static_cast<uint32_t>((clipMax.x - clipMin.x)), static_cast<uint32_t>((clipMax.y - clipMin.y))}
                });
                primitive->args.emplace_back(rhi::CmdDrawIndexed {
                    pCmd->ElemCount,
                    1,
                    pCmd->IdxOffset + globalIdxOffset,
                    static_cast<int32_t>(pCmd->VtxOffset + globalVtxOffset),
                    0
                });
            }
            globalVtxOffset += cmdList->VtxBuffer.Size;
            globalIdxOffset += cmdList->IdxBuffer.Size;
        }

        stagingBuffer->GetRHIBuffer()->UnMap();
        rdg.AddUploadPass("UploadImGuiVtx", {stagingBuffer, vertexBuffer, 0, 0, vertexSize});
        rdg.AddUploadPass("UploadImGuiIdx", {stagingBuffer, indexBuffer, vertexSize, 0, indexSize});
    }

    void ImGuiInstance::MakeCurrent()
    {
        ImGui::SetCurrentContext(context);
    }

    void ImGuiInstance::BindNativeWindow(const NativeWindow *nativeWindow)
    {
        Event<IWindowEvent>::Connect(nativeWindow, this);

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(nativeWindow->GetWidth()), static_cast<float>(nativeWindow->GetHeight()));
        io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
    }

    void ImGuiInstance::OnMouseWheel(int32_t wheelX, int32_t wheelY)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (wheelX > 0) { io.MouseWheelH += 1; }
        if (wheelX < 0) { io.MouseWheelH -= 1; }
        if (wheelY > 0) { io.MouseWheel += 1; }
        if (wheelY < 0) { io.MouseWheel -= 1; }
    }

    void ImGuiInstance::OnFocusChanged(bool focus)
    {
        MakeCurrent();
        ImGuiIO& io = ImGui::GetIO();
        io.AddFocusEvent(focus);
    }

    void ImGuiInstance::OnWindowResize(uint32_t width, uint32_t height)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
    }

    void ImGuiInstance::OnTextInput(const char *text)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddInputCharactersUTF8(text);
    }

    void ImGuiInstance::OnMouseMove(int32_t x, int32_t y)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MousePos = ImVec2(static_cast<float>(x), static_cast<float>(y));
    }

    void ImGuiInstance::OnMouseButtonDown(MouseButtonType button)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[button - 1] = true;
    }

    void ImGuiInstance::OnMouseButtonUp(MouseButtonType button)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[button - 1] = false;
    }

    void ImGuiInstance::CheckVertexBuffers(uint32_t vertexCount, uint32_t indexCount)
    {
        bool rebuildVA = false;
        auto vs = vertexCount * sizeof(ImDrawVert);
        auto is = indexCount * sizeof(ImDrawIdx);

        if (vs > vertexSize) {
            vertexSize = std::max(vertexSize * 2, vs);
            if (!vertexBuffer) {
                vertexBuffer = std::make_shared<Buffer>();
                vertexBuffer->Init(vertexSize, rhi::BufferUsageFlagBit::TRANSFER_DST | rhi::BufferUsageFlagBit::VERTEX, rhi::MemoryType::GPU_ONLY);
            } else {
                vertexBuffer->Resize(vertexSize);
            }
            rebuildVA = true;
        }

        if (is > indexSize) {
            indexSize = std::max(indexSize * 2, is);
            if (!indexBuffer) {
                indexBuffer = std::make_shared<Buffer>();
                indexBuffer->Init(indexSize, rhi::BufferUsageFlagBit::TRANSFER_DST | rhi::BufferUsageFlagBit::INDEX, rhi::MemoryType::GPU_ONLY);
            } else {
                indexBuffer->Resize(indexSize);
            }
            rebuildVA = true;
        }

        if (rebuildVA) {
            rhi::VertexAssembly::Descriptor desc = {};
            desc.vertexBuffers.emplace_back(vertexBuffer->GetRHIBuffer()->CreateView({0, vertexSize}));
            desc.indexBuffer = indexBuffer->GetRHIBuffer()->CreateView({0, indexSize});
            desc.indexType = sizeof(ImDrawIdx) == 2 ? rhi::IndexType::U16 : rhi::IndexType::U32;

            primitive->va = RHI::Get()->GetDevice()->CreateVertexAssembly(desc);

            // rebuild staging buffer
            if (!stagingBuffer) {
                stagingBuffer = std::make_shared<Buffer>();
                stagingBuffer->Init(vertexSize + indexSize, rhi::BufferUsageFlagBit::TRANSFER_SRC, rhi::MemoryType::CPU_TO_GPU);
            } else {
                stagingBuffer->Resize(vertexSize + indexSize);
            }
        }
    }

    void ImGuiInstance::AttachScene(RenderScene *scene)
    {
        if (renderScene != nullptr) {
            renderScene->RemovePrimitive(primitive.get());
        } else {
            renderScene = scene;
            renderScene->AddPrimitive(primitive.get());
        }
    }

    void ImGuiInstance::DetachScene()
    {
        if (renderScene != nullptr) {
            renderScene->RemovePrimitive(primitive.get());
        }
        renderScene = nullptr;
    }
} // namespace sky