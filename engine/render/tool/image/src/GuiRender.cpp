//
// Created by blues on 2025/10/3.
//

#include "GuiRender.h"
#include "shader/ShaderCompilerGlsl.h"
#include <framework/window/NativeWindowManager.h>
#include <rhi/Stream.h>
#include <rhi/Queue.h>
#include <render/RHI.h>
#include <imgui.h>

#include <algorithm>

namespace sky {

    struct UITransform {
        ImVec2 scale;
        ImVec2 translate;
    };


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

    extern std::unique_ptr<ShaderCompilerGlsl> GCompiler;

    GuiRender::GuiRender(rhi::Device* dev, NativeWindow *window, const rhi::RenderPassPtr &pass) : device(dev)
    {
        binder.Bind(this, window);
        mouseBinder.Bind(this);

        IMGUI_CHECKVERSION();

        std::vector<rhi::ShaderPtr> shaders(2);
        rhi::Shader::Descriptor shaderDesc = {};

        ShaderSourceDesc desc = {};
        desc.entry = "main";

        ShaderCompileOption op = {ShaderCompileTarget::SPIRV, ShaderLanguage::GLSL};
        ShaderBuildResult result;
        {
            desc.source = ""
                          "layout(location = 0) in vec2 aPos;\n"
                          "layout(location = 1) in vec2 aUV;\n"
                          "layout(location = 2) in vec4 aColor;\n"
                          "layout(location = 0) out vec4 color;\n"
                          "layout(location = 1) out vec2 uv;\n"
                          "layout(binding = 0) uniform Ubo {\n"
                          "    vec2 Scale;\n"
                          "    vec2 Translate;\n"
                          "} pc;\n"
                          "void main() {\n"
                          "    color = aColor;\n"
                          "    uv = aUV;\n"
                          "    gl_Position = vec4(aPos * pc.Scale + pc.Translate, 0, 1);"
                          "}";
            desc.stage = rhi::ShaderStageFlagBit::VS;
            result.data.clear();
            GCompiler->CompileBinary(desc, op, result);

            shaderDesc.data = reinterpret_cast<const uint8_t*>(result.data.data());
            shaderDesc.size = static_cast<uint32_t>(result.data.size() * sizeof(uint32_t));
            shaderDesc.stage = desc.stage;
            shaders[0] = device->CreateShader(shaderDesc);
            shaders[0]->SetEntry("main");
        }

        {
            desc.source = ""
                          "layout(binding = 1) uniform sampler2D sTexture;\n"
                          "layout(location = 0) out vec4 color;\n"
                          "layout(location = 0) in vec4 inColor;\n"
                          "layout(location = 1) in vec2 inUv;\n"
                          "void main() {\n"
                          "    color = inColor * texture(sTexture, inUv);\n"
                          "}";
            desc.stage = rhi::ShaderStageFlagBit::FS;
            result.data.clear();
            GCompiler->CompileBinary(desc, op, result);

            shaderDesc.data = reinterpret_cast<const uint8_t*>(result.data.data());
            shaderDesc.size = static_cast<uint32_t>(result.data.size() * sizeof(uint32_t));
            shaderDesc.stage = desc.stage;
            shaders[1] = device->CreateShader(shaderDesc);
            shaders[1]->SetEntry("main");
        }

        rhi::DescriptorSetLayout::Descriptor setLayoutDesc = {};
        setLayoutDesc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding{.type=rhi::DescriptorType::UNIFORM_BUFFER, .count=1, .binding=0, .visibility=rhi::ShaderStageFlagBit::VS});
        setLayoutDesc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding{.type=rhi::DescriptorType::COMBINED_IMAGE_SAMPLER, .count=1, .binding=1, .visibility=rhi::ShaderStageFlagBit::FS});

        rhi::PipelineLayout::Descriptor layoutDesc = {};
        layoutDesc.layouts.emplace_back(device->CreateDescriptorSetLayout(setLayoutDesc));

        layout = device->CreatePipelineLayout(layoutDesc);

        std::vector<rhi::VertexAttributeDesc> attributes = {
            {.location=0, .binding=0, .offset=0, .format=rhi::Format::F_RG32},
            {.location=1, .binding=0, .offset=8, .format=rhi::Format::F_RG32},
            {.location=2, .binding=0, .offset=16, .format=rhi::Format::F_RGBA8},
        };
        std::vector<rhi::VertexBindingDesc> bindings = {
            {.binding=0, .stride=sizeof(ImDrawVert), .inputRate=rhi::VertexInputRate::PER_VERTEX }
        };
        rhi::VertexInput::Descriptor vertexDesc = {};
        vertexDesc.attributesNum = 3;
        vertexDesc.attributes = attributes.data();
        vertexDesc.bindingsNum = 1;
        vertexDesc.bindings = bindings.data();

        rhi::BlendState blendState = {};
        blendState.blendEn = true;
        blendState.srcColor = rhi::BlendFactor::SRC_ALPHA;
        blendState.dstColor = rhi::BlendFactor::ONE_MINUS_SRC_ALPHA;
        blendState.srcAlpha = rhi::BlendFactor::ONE;
        blendState.dstAlpha = rhi::BlendFactor::ONE_MINUS_SRC_ALPHA;

        rhi::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.state.blendStates.emplace_back(blendState);
        psoDesc.vs = shaders[0];
        psoDesc.fs = shaders[1];
        psoDesc.vertexInput = device->CreateVertexInput(vertexDesc);
        psoDesc.renderPass = pass;
        psoDesc.pipelineLayout = layout;

        pso = device->CreateGraphicsPipeline(psoDesc);

        rhi::DescriptorSetPool::PoolSize Sizes[2];
        Sizes[0] = {.type=rhi::DescriptorType::UNIFORM_BUFFER, .count=1};
        Sizes[1] = {.type=rhi::DescriptorType::COMBINED_IMAGE_SAMPLER, .count=1};
        rhi::DescriptorSetPool::Descriptor poolDesc = {.maxSets=1, .sizeCount=2, .sizeData=Sizes};
        setPool = device->CreateDescriptorSetPool(poolDesc);

        rhi::DescriptorSet::Descriptor setDesc = {layoutDesc.layouts[0]};
        set = setPool->Allocate(setDesc);

        rhi::Buffer::Descriptor uboDesc = {};
        uboDesc.size = sizeof(UITransform);
        uboDesc.usage = rhi::BufferUsageFlagBit::UNIFORM;
        uboDesc.memory = rhi::MemoryType::CPU_TO_GPU;
        ubo = device->CreateBuffer(uboDesc);

        ImGui::CreateContext();
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
        io.KeyMap[ImGuiKey_A]           = static_cast<int>(ScanCode::KEY_A);
        io.KeyMap[ImGuiKey_C]           = static_cast<int>(ScanCode::KEY_C);
        io.KeyMap[ImGuiKey_V]           = static_cast<int>(ScanCode::KEY_V);
        io.KeyMap[ImGuiKey_X]           = static_cast<int>(ScanCode::KEY_X);
        io.KeyMap[ImGuiKey_Y]           = static_cast<int>(ScanCode::KEY_Y);
        io.KeyMap[ImGuiKey_Z]           = static_cast<int>(ScanCode::KEY_Z);

        io.DisplaySize = ImVec2(static_cast<float>(window->GetWidth()), static_cast<float>(window->GetHeight()));
        io.DisplayFramebufferScale = ImVec2(1.f, 1.f);

        unsigned char *pixels;
        int width;
        int height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        rhi::Image::Descriptor fontImage = {};
        fontImage.imageType   = rhi::ImageType::IMAGE_2D;
        fontImage.format      = rhi::PixelFormat::RGBA8_UNORM;
        fontImage.extent      = {.width=static_cast<uint32_t>(width), .height=static_cast<uint32_t>(height), .depth=1};
        fontImage.mipLevels   = 1;
        fontImage.arrayLayers = 1;
        fontImage.samples     = rhi::SampleCount::X1;
        fontImage.usage       = rhi::ImageUsageFlagBit::SAMPLED | rhi::ImageUsageFlagBit::TRANSFER_DST;
        fontImage.memory      = rhi::MemoryType::GPU_ONLY;
        image = device->CreateImage(fontImage);
        font = image->CreateView({});

        {
            rhi::ImageUploadRequest uploadRequest = {};
            uploadRequest.source = new rhi::RawPtrStream(pixels);
            uploadRequest.size = width * height * 4 * sizeof(char);
            uploadRequest.imageExtent = fontImage.extent;

            auto* queue = device->GetQueue(rhi::QueueType::TRANSFER);
            auto handle = queue->UploadImage(image, uploadRequest);
            queue->Wait(handle);
        }

        set->BindBuffer(0, ubo, 0, sizeof(UITransform), 0);
        set->BindImageView(1, font, 0);
        set->Update();
    }

    void GuiRender::Tick(float delta)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DeltaTime = delta;

        ImGui::NewFrame();

        for (auto *widget : widgets) {
            widget->Execute();;
        }

        ImGui::EndFrame();
        ImGui::Render();
    }

    void GuiRender::Render(rhi::GraphicsEncoder & encoder)
    {
        auto* drawData = ImGui::GetDrawData();

        UITransform transform = {};
        transform.scale.x = 2.0f / drawData->DisplaySize.x;
        transform.scale.y = 2.0f / drawData->DisplaySize.y;
        transform.translate.x = -1.0f - drawData->DisplayPos.x * transform.scale.x;
        transform.translate.y = -1.0f - drawData->DisplayPos.y * transform.scale.y;

        uint8_t *ptr = ubo->Map();
        memcpy(ptr, &transform, sizeof(UITransform));

        float fbWidth  = drawData->DisplaySize.x * drawData->FramebufferScale.x;
        float fbHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;

        ImVec2 clipOff   = drawData->DisplayPos;
        ImVec2 clipScale = drawData->FramebufferScale;

        int globalVtxOffset = 0;
        int globalIdxOffset = 0;

        if (drawData->TotalIdxCount == 0 || drawData->TotalVtxCount == 0) {
            return;
        }

        CheckSize(drawData->TotalVtxCount, drawData->TotalIdxCount);

        auto *vtxDst = reinterpret_cast<ImDrawVert *>(vertexBuffer->Map());
        auto *idxDst = reinterpret_cast<ImDrawIdx *>(indexBuffer->Map());

        std::vector<rhi::BufferView> vertexBuffers;
        vertexBuffers.emplace_back(rhi::BufferView{
            .buffer=vertexBuffer, .offset=0, .range=vertexSize
        });
        rhi::BufferView ib {
            .buffer=indexBuffer, .offset=0, .range=indexSize
        };

        encoder.BindPipeline(pso);
        encoder.BindSet(0, set);
        encoder.BindVertexBuffers(vertexBuffers);
        encoder.BindIndexBuffer(ib, sizeof(ImDrawIdx) == 2 ? rhi::IndexType::U16 : rhi::IndexType::U32);

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

                clipMin.x = std::max(clipMin.x, 0.0f);
                clipMin.y = std::max(clipMin.y, 0.0f);
                clipMax.x = std::min(clipMax.x, fbWidth);
                clipMax.y = std::min(clipMax.y, fbHeight);
                if (clipMax.x < clipMin.x || clipMax.y < clipMin.y) { continue; }

                auto scissor = rhi::Rect2D{
                    .offset={.x=static_cast<int32_t>((clipMin.x)), .y=static_cast<int32_t>((clipMin.y))},
                    .extent={.width=static_cast<uint32_t>((clipMax.x - clipMin.x)), .height=static_cast<uint32_t>((clipMax.y - clipMin.y))}
                };

                auto arg = rhi::CmdDrawIndexed {
                    .indexCount=pCmd->ElemCount,
                    .instanceCount=1,
                    .firstIndex=pCmd->IdxOffset + globalIdxOffset,
                    .vertexOffset=static_cast<int32_t>(pCmd->VtxOffset + globalVtxOffset),
                    .firstInstance=0
                };

                encoder.SetScissor(1, &scissor);
                encoder.DrawIndexed(arg);
            }
            globalVtxOffset += cmdList->VtxBuffer.Size;
            globalIdxOffset += cmdList->IdxBuffer.Size;
        }
    }

    void GuiRender::CheckSize(uint32_t vertexCount, uint32_t indexCount)
    {
        auto vs = vertexCount * sizeof(ImDrawVert);
        auto is = indexCount * sizeof(ImDrawIdx);

        rhi::Buffer::Descriptor bufferDesc = {};
        bufferDesc.memory = rhi::MemoryType::CPU_TO_GPU;

        if (vs > vertexSize) {
            vertexSize = std::max(vertexSize * 2, static_cast<uint32_t>(vs));
            bufferDesc.size = vertexSize;
            bufferDesc.usage = rhi::BufferUsageFlagBit::VERTEX;
            vertexBuffer = device->CreateBuffer(bufferDesc);
        }

        if (is > indexSize) {
            indexSize = std::max(indexSize * 2, static_cast<uint32_t>(is));
            bufferDesc.size = indexSize;
            bufferDesc.usage = rhi::BufferUsageFlagBit::INDEX;
            indexBuffer = device->CreateBuffer(bufferDesc);
        }
    }

    void GuiRender::OnWindowResize(const WindowResizeEvent& event)
    {
        auto *window = NativeWindowManager::Get()->GetWindowByID(event.winID);
        SKY_ASSERT(window != nullptr);

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(event.width), static_cast<float>(event.height));
        io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
    }

    void GuiRender::OnFocusChanged(bool focus)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddFocusEvent(focus);
    }

    void GuiRender::OnMouseButtonDown(const MouseButtonEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[ConvertMouseButton(event.button)] = true;
    }

    void GuiRender::OnMouseButtonUp(const MouseButtonEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[ConvertMouseButton(event.button)] = false;
    }

    void GuiRender::OnMouseMotion(const MouseMotionEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MousePos = ImVec2(static_cast<float>(event.x), static_cast<float>(event.y));
    }

    void GuiRender::OnMouseWheel(const MouseWheelEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (event.x > 0) { io.MouseWheelH += 1; }
        if (event.x < 0) { io.MouseWheelH -= 1; }
        if (event.y > 0) { io.MouseWheel += 1; }
        if (event.y < 0) { io.MouseWheel -= 1; }
    }

    GuiRender::~GuiRender()
    {
        pso = nullptr;
        layout = nullptr;
        setPool = nullptr;
        ubo = nullptr;
        image = nullptr;
        font = nullptr;
        vertexBuffer = nullptr;
        indexBuffer = nullptr;
    }

} // namespace sky