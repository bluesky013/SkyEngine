//
// Created by blues on 2023/9/20.
//

#include <imgui/ImGuiInstance.h>
#include <framework/window/NativeWindow.h>
#include <framework/platform/PlatformBase.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <render/RenderBase.h>
#include <imgui/ImGuiFeature.h>
#include <render/rdg/RenderGraph.h>

#include <imgui.h>

namespace sky {
    struct UITransform {
        ImVec2 scale;
        ImVec2 translate;
    };

    void ImContext::Init()
    {
        imContext = ImGui::CreateContext();
//        plotContext = ImPlot::CreateContext();
    }

    void ImContext::Destroy()
    {
        ImGui::DestroyContext(imContext);
        imContext = nullptr;

//        ImPlot::DestroyContext(plotContext);
//        plotContext = nullptr;
    }

    void ImContext::MakeCurrent() const
    {
        ImGui::SetCurrentContext(imContext);
//        ImPlot::SetCurrentContext(plotContext);
    }

    void ImguiPrimitive::UpdateBatch()
    {
        auto &batch = batches[0];
        if (!batch.batchGroup) {
            ubo = new DynamicUniformBuffer();
            ubo->Init(sizeof(UITransform));

            auto layout = batch.program->RequestLayout(BATCH_SET);
            batch.batchGroup = new ResourceGroup();
            batch.batchGroup->Init(layout, *ImGuiFeature::Get()->GetPool());
            batch.batchGroup->BindTexture(Name("FontTexture"), fontTexture->GetImageView(), 0);
            batch.batchGroup->BindDynamicUBO(Name("Constants"), ubo, 0);
            batch.batchGroup->Update();
        }
    }

    ImGuiInstance::ImGuiInstance()
    {
        IMGUI_CHECKVERSION();
        context.Init();
        context.MakeCurrent();

        ImGuiIO& io = ImGui::GetIO();

        io.KeyMap[ImGuiKey_Tab]         = static_cast<int>(ScanCode::KEY_TAB);
        io.KeyMap[ImGuiKey_LeftArrow]   = static_cast<int>(ScanCode::KEY_LEFT);
        io.KeyMap[ImGuiKey_RightArrow]  = static_cast<int>(ScanCode::KEY_RIGHT);
        io.KeyMap[ImGuiKey_UpArrow]     = static_cast<int>(ScanCode::KEY_UP);
        io.KeyMap[ImGuiKey_DownArrow]   = static_cast<int>(ScanCode::KEY_DOWN);
        io.KeyMap[ImGuiKey_PageUp]      = static_cast<int>(ScanCode::KEY_PAGEUP);
        io.KeyMap[ImGuiKey_PageDown]    = static_cast<int>(ScanCode::KEY_PAGEDOWN);
        io.KeyMap[ImGuiKey_Home]        = static_cast<int>(ScanCode::KEY_HOME);
        io.KeyMap[ImGuiKey_End]         = static_cast<int>(ScanCode::KEY_END);
        io.KeyMap[ImGuiKey_Insert]      = static_cast<int>(ScanCode::KEY_INSERT);
        io.KeyMap[ImGuiKey_Delete]      = static_cast<int>(ScanCode::KEY_DELETE);
        io.KeyMap[ImGuiKey_Backspace]   = static_cast<int>(ScanCode::KEY_BACKSPACE);
        io.KeyMap[ImGuiKey_Space]       = static_cast<int>(ScanCode::KEY_SPACE);
        io.KeyMap[ImGuiKey_Enter]       = static_cast<int>(ScanCode::KEY_RETURN);
        io.KeyMap[ImGuiKey_Escape]      = static_cast<int>(ScanCode::KEY_ESCAPE);
        io.KeyMap[ImGuiKey_KeypadEnter] = static_cast<int>(ScanCode::KEY_KP_ENTER);
        io.KeyMap[ImGuiKey_A]           = static_cast<int>(ScanCode::KEY_A);
        io.KeyMap[ImGuiKey_C]           = static_cast<int>(ScanCode::KEY_C);
        io.KeyMap[ImGuiKey_V]           = static_cast<int>(ScanCode::KEY_V);
        io.KeyMap[ImGuiKey_X]           = static_cast<int>(ScanCode::KEY_X);
        io.KeyMap[ImGuiKey_Y]           = static_cast<int>(ScanCode::KEY_Y);
        io.KeyMap[ImGuiKey_Z]           = static_cast<int>(ScanCode::KEY_Z);


        io.SetClipboardTextFn = [](void* user_data, const char* text){
            Platform::Get()->SetClipBoardText(text);
        };
        io.GetClipboardTextFn = [](void* user_data)-> const char* {
            auto &io = ImGui::GetIO();
            if (io.ClipboardUserData != nullptr) {
                Platform::Get()->FreeClipBoardText(static_cast<char*>(io.ClipboardUserData));
            }
            auto *text = Platform::Get()->GetClipBoardText();
            io.ClipboardUserData = static_cast<char*>(text);
            return text;
        };

        unsigned char *pixels;
        int width;
        int height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        uint32_t uploadSize = width * height * 4 * sizeof(char);
        std::vector<uint8_t> rawData(uploadSize);
        memcpy(rawData.data(), pixels, uploadSize);

        auto *feature = ImGuiFeature::Get();
        primitive = std::make_unique<ImguiPrimitive>();
        primitive->geometry = new RenderGeometry();
        primitive->geometry->AddVertexAttribute(VertexAttribute{VertexSemanticFlagBit::POSITION, 0, OFFSET_OF(ImDrawVert, pos), rhi::Format::F_RG32});
        primitive->geometry->AddVertexAttribute(VertexAttribute{VertexSemanticFlagBit::UV,       0, OFFSET_OF(ImDrawVert, uv),  rhi::Format::F_RG32});
        primitive->geometry->AddVertexAttribute(VertexAttribute{VertexSemanticFlagBit::COLOR,    0, OFFSET_OF(ImDrawVert, col), rhi::Format::F_RGBA8});
        primitive->batches.emplace_back(feature->GetTechnique());

        primitive->ubo = new DynamicUniformBuffer();
        primitive->ubo->Init(sizeof(UITransform));

        auto *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
        primitive->fontTexture = new Texture2D();
        primitive->fontTexture->Init(rhi::PixelFormat::RGBA8_UNORM, width, height, 1);
        primitive->fontTexture->Upload(std::move(rawData), queue);
        io.Fonts->SetTexID((ImTextureID)(intptr_t)primitive->fontTexture->GetImageView().get());
    }

    ImGuiInstance::~ImGuiInstance()
    {
        context.Destroy();

        primitive     = nullptr;
        vertexBuffer  = nullptr;
        indexBuffer   = nullptr;
        globalSet     = nullptr;
    }

    void ImGuiInstance::AddWidget(ImWidget *widget)
    {
        widgets.emplace_back(widget);
    }

    void ImGuiInstance::RemoveWidget(ImWidget *widget)
    {
        auto iter = std::find_if(widgets.begin(), widgets.end(), [widget](const auto &wgt) -> bool  {
            return widget == wgt;
        });
        if (iter != widgets.end()) {
            widgets.erase(iter);
        }
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
    }

    void ImGuiInstance::Render(rdg::RenderGraph &rdg)
    {
        if (!primitive || !primitive->fontTexture->IsReady()) {
            return;
        }

        MakeCurrent();

        drawData = ImGui::GetDrawData();
        UITransform transform;
        transform.scale.x = 2.0f / drawData->DisplaySize.x;
        transform.scale.y = 2.0f / drawData->DisplaySize.y;
        transform.translate.x = -1.0f - drawData->DisplayPos.x * transform.scale.x;
        transform.translate.y = -1.0f - drawData->DisplayPos.y * transform.scale.y;
        primitive->ubo->WriteT(0, transform);
        primitive->ubo->Upload();

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

        vertexBuffer->SwapBuffer();
        auto *vtxDst = reinterpret_cast<ImDrawVert *>(vertexBuffer->GetMapped());

        indexBuffer->SwapBuffer();
        auto *idxDst = reinterpret_cast<ImDrawIdx *>(indexBuffer->GetMapped());

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
    }

    void ImGuiInstance::MakeCurrent()
    {
        context.MakeCurrent();
    }

    void ImGuiInstance::BindNativeWindow(const NativeWindow *nativeWindow)
    {
        winBinder.Bind(this, nativeWindow);
        mouseBinder.Bind(this);
        keyBinder.Bind(this);

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(nativeWindow->GetWidth()), static_cast<float>(nativeWindow->GetHeight()));
        io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
    }

    int ConvertMouseButton(MouseButtonType type)
    {
        // 0=left, 1=right, 2=middle
        switch (type) {
            case MouseButtonType::LEFT:
                return 0;
            case MouseButtonType::RIGHT:
                return 1;
            case MouseButtonType::MIDDLE:
                return 2;
            default:
                return 0;
        }
    }

    void ImGuiInstance::OnMouseButtonDown(const MouseButtonEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[ConvertMouseButton(event.button)] = true;
    }

    void ImGuiInstance::OnMouseButtonUp(const MouseButtonEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[ConvertMouseButton(event.button)] = false;
    }

    void ImGuiInstance::OnMouseMotion(const MouseMotionEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MousePos = ImVec2(static_cast<float>(event.x), static_cast<float>(event.y));
    }

    void ImGuiInstance::OnMouseWheel(const MouseWheelEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (event.x > 0) { io.MouseWheelH += 1; }
        if (event.x < 0) { io.MouseWheelH -= 1; }
        if (event.y > 0) { io.MouseWheel += 1; }
        if (event.y < 0) { io.MouseWheel -= 1; }
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

    void ImGuiInstance::OnKeyUp(const KeyboardEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.KeysDown[static_cast<uint32_t>(event.scanCode)] = false;
        io.KeyShift = static_cast<bool>(event.mod & KeyMod::SHIFT);
        io.KeyCtrl = static_cast<bool>(event.mod & KeyMod::CTRL);
        io.KeyAlt = static_cast<bool>(event.mod & KeyMod::ALT);
    }

    void ImGuiInstance::OnKeyDown(const KeyboardEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.KeysDown[static_cast<uint32_t>(event.scanCode)] = true;
        io.KeyShift = static_cast<bool>(event.mod & KeyMod::SHIFT);
        io.KeyCtrl = static_cast<bool>(event.mod & KeyMod::CTRL);
        io.KeyAlt = static_cast<bool>(event.mod & KeyMod::ALT);
    }

    void ImGuiInstance::OnTextInput(WindowID winID, const char *text)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddInputCharactersUTF8(text);
    }

    void ImGuiInstance::CheckVertexBuffers(uint32_t vertexCount, uint32_t indexCount)
    {
        bool rebuildGeometry = false;
        auto vs = vertexCount * sizeof(ImDrawVert);
        auto is = indexCount * sizeof(ImDrawIdx);

        if (vs > vertexSize) {
            vertexSize = std::max(vertexSize * 2, static_cast<uint32_t>(vs));
            vertexBuffer = new DynamicBuffer();
            vertexBuffer->Init(vertexSize, rhi::BufferUsageFlagBit::VERTEX);
            rebuildGeometry = true;
        }

        if (is > indexSize) {
            indexSize = std::max(indexSize * 2, static_cast<uint32_t>(is));
            indexBuffer = new DynamicBuffer();
            indexBuffer->Init(indexSize, rhi::BufferUsageFlagBit::INDEX);
            rebuildGeometry = true;
        }

        if (rebuildGeometry) {
            primitive->geometry->Reset();
            primitive->geometry->vertexBuffers.emplace_back(
                VertexBuffer{vertexBuffer, 0, vertexBuffer->GetSize(), static_cast<uint32_t>(sizeof(ImDrawVert))}
            );
            primitive->geometry->indexBuffer =
                IndexBuffer{indexBuffer, 0, indexBuffer->GetSize(), sizeof(ImDrawIdx) == 2 ? rhi::IndexType::U16 : rhi::IndexType::U32}; // NOLINT
            primitive->geometry->dynamicVB = true;
            primitive->geometry->version++;
        }
    }
} // namespace sky
