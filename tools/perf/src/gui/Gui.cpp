//
// Created by Zach Lee on 2023/4/22.
//

#include <perf/gui/Gui.h>
#include <imgui.h>
#include <framework/platform/PlatformBase.h>

namespace sky::perf {
    namespace {
        //-----------------------------------------------------------------------------
        // SHADERS
        //-----------------------------------------------------------------------------

        // glsl_shader.vert, compiled with:
        // # glslangValidator -V -x -o glsl_shader.vert.u32 glsl_shader.vert
        /*
        #version 450 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUV;
        layout(location = 2) in vec4 aColor;
        layout(push_constant) uniform uPushConstant { vec2 uScale; vec2 uTranslate; } pc;

        out gl_PerVertex { vec4 gl_Position; };
        layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;

        void main()
        {
            Out.Color = aColor;
            Out.UV = aUV;
            gl_Position = vec4(aPos * pc.uScale + pc.uTranslate, 0, 1);
        }
        */
        static std::vector<uint32_t> VS_SPV =
            {
                0x07230203,0x00010000,0x00080001,0x0000002e,0x00000000,0x00020011,0x00000001,0x0006000b,
                0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
                0x000a000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000b,0x0000000f,0x00000015,
                0x0000001b,0x0000001c,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
                0x00000000,0x00030005,0x00000009,0x00000000,0x00050006,0x00000009,0x00000000,0x6f6c6f43,
                0x00000072,0x00040006,0x00000009,0x00000001,0x00005655,0x00030005,0x0000000b,0x0074754f,
                0x00040005,0x0000000f,0x6c6f4361,0x0000726f,0x00030005,0x00000015,0x00565561,0x00060005,
                0x00000019,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x00000019,0x00000000,
                0x505f6c67,0x7469736f,0x006e6f69,0x00030005,0x0000001b,0x00000000,0x00040005,0x0000001c,
                0x736f5061,0x00000000,0x00060005,0x0000001e,0x73755075,0x6e6f4368,0x6e617473,0x00000074,
                0x00050006,0x0000001e,0x00000000,0x61635375,0x0000656c,0x00060006,0x0000001e,0x00000001,
                0x61725475,0x616c736e,0x00006574,0x00030005,0x00000020,0x00006370,0x00040047,0x0000000b,
                0x0000001e,0x00000000,0x00040047,0x0000000f,0x0000001e,0x00000002,0x00040047,0x00000015,
                0x0000001e,0x00000001,0x00050048,0x00000019,0x00000000,0x0000000b,0x00000000,0x00030047,
                0x00000019,0x00000002,0x00040047,0x0000001c,0x0000001e,0x00000000,0x00050048,0x0000001e,
                0x00000000,0x00000023,0x00000000,0x00050048,0x0000001e,0x00000001,0x00000023,0x00000008,
                0x00030047,0x0000001e,0x00000002,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
                0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040017,
                0x00000008,0x00000006,0x00000002,0x0004001e,0x00000009,0x00000007,0x00000008,0x00040020,
                0x0000000a,0x00000003,0x00000009,0x0004003b,0x0000000a,0x0000000b,0x00000003,0x00040015,
                0x0000000c,0x00000020,0x00000001,0x0004002b,0x0000000c,0x0000000d,0x00000000,0x00040020,
                0x0000000e,0x00000001,0x00000007,0x0004003b,0x0000000e,0x0000000f,0x00000001,0x00040020,
                0x00000011,0x00000003,0x00000007,0x0004002b,0x0000000c,0x00000013,0x00000001,0x00040020,
                0x00000014,0x00000001,0x00000008,0x0004003b,0x00000014,0x00000015,0x00000001,0x00040020,
                0x00000017,0x00000003,0x00000008,0x0003001e,0x00000019,0x00000007,0x00040020,0x0000001a,
                0x00000003,0x00000019,0x0004003b,0x0000001a,0x0000001b,0x00000003,0x0004003b,0x00000014,
                0x0000001c,0x00000001,0x0004001e,0x0000001e,0x00000008,0x00000008,0x00040020,0x0000001f,
                0x00000009,0x0000001e,0x0004003b,0x0000001f,0x00000020,0x00000009,0x00040020,0x00000021,
                0x00000009,0x00000008,0x0004002b,0x00000006,0x00000028,0x00000000,0x0004002b,0x00000006,
                0x00000029,0x3f800000,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,
                0x00000005,0x0004003d,0x00000007,0x00000010,0x0000000f,0x00050041,0x00000011,0x00000012,
                0x0000000b,0x0000000d,0x0003003e,0x00000012,0x00000010,0x0004003d,0x00000008,0x00000016,
                0x00000015,0x00050041,0x00000017,0x00000018,0x0000000b,0x00000013,0x0003003e,0x00000018,
                0x00000016,0x0004003d,0x00000008,0x0000001d,0x0000001c,0x00050041,0x00000021,0x00000022,
                0x00000020,0x0000000d,0x0004003d,0x00000008,0x00000023,0x00000022,0x00050085,0x00000008,
                0x00000024,0x0000001d,0x00000023,0x00050041,0x00000021,0x00000025,0x00000020,0x00000013,
                0x0004003d,0x00000008,0x00000026,0x00000025,0x00050081,0x00000008,0x00000027,0x00000024,
                0x00000026,0x00050051,0x00000006,0x0000002a,0x00000027,0x00000000,0x00050051,0x00000006,
                0x0000002b,0x00000027,0x00000001,0x00070050,0x00000007,0x0000002c,0x0000002a,0x0000002b,
                0x00000028,0x00000029,0x00050041,0x00000011,0x0000002d,0x0000001b,0x0000000d,0x0003003e,
                0x0000002d,0x0000002c,0x000100fd,0x00010038
        };

        // glsl_shader.frag, compiled with:
        // # glslangValidator -V -x -o glsl_shader.frag.u32 glsl_shader.frag
        /*
        #version 450 core
        layout(location = 0) out vec4 fColor;
        layout(set=0, binding=0) uniform sampler2D sTexture;
        layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
        void main()
        {
            fColor = In.Color * texture(sTexture, In.UV.st);
        }
        */
        static std::vector<uint32_t> FS_SPV =
            {
                0x07230203,0x00010000,0x00080001,0x0000001e,0x00000000,0x00020011,0x00000001,0x0006000b,
                0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
                0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000d,0x00030010,
                0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
                0x00000000,0x00040005,0x00000009,0x6c6f4366,0x0000726f,0x00030005,0x0000000b,0x00000000,
                0x00050006,0x0000000b,0x00000000,0x6f6c6f43,0x00000072,0x00040006,0x0000000b,0x00000001,
                0x00005655,0x00030005,0x0000000d,0x00006e49,0x00050005,0x00000016,0x78655473,0x65727574,
                0x00000000,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000d,0x0000001e,
                0x00000000,0x00040047,0x00000016,0x00000022,0x00000000,0x00040047,0x00000016,0x00000021,
                0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,
                0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,
                0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040017,0x0000000a,0x00000006,
                0x00000002,0x0004001e,0x0000000b,0x00000007,0x0000000a,0x00040020,0x0000000c,0x00000001,
                0x0000000b,0x0004003b,0x0000000c,0x0000000d,0x00000001,0x00040015,0x0000000e,0x00000020,
                0x00000001,0x0004002b,0x0000000e,0x0000000f,0x00000000,0x00040020,0x00000010,0x00000001,
                0x00000007,0x00090019,0x00000013,0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,
                0x00000001,0x00000000,0x0003001b,0x00000014,0x00000013,0x00040020,0x00000015,0x00000000,
                0x00000014,0x0004003b,0x00000015,0x00000016,0x00000000,0x0004002b,0x0000000e,0x00000018,
                0x00000001,0x00040020,0x00000019,0x00000001,0x0000000a,0x00050036,0x00000002,0x00000004,
                0x00000000,0x00000003,0x000200f8,0x00000005,0x00050041,0x00000010,0x00000011,0x0000000d,
                0x0000000f,0x0004003d,0x00000007,0x00000012,0x00000011,0x0004003d,0x00000014,0x00000017,
                0x00000016,0x00050041,0x00000019,0x0000001a,0x0000000d,0x00000018,0x0004003d,0x0000000a,
                0x0000001b,0x0000001a,0x00050057,0x00000007,0x0000001c,0x00000017,0x0000001b,0x00050085,
                0x00000007,0x0000001d,0x00000012,0x0000001c,0x0003003e,0x00000009,0x0000001d,0x000100fd,
                0x00010038
        };
    }


    Gui::~Gui()
    {

        render->device->WaitIdle();

        res = nullptr;
        render->Stop();
        render = nullptr;
        ImGui::DestroyContext();
    }

    void Gui::Init()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;

        render = std::make_unique<Render>();
        render->Init();

        res = std::make_unique<UIRes>();
        InitFont();
        InitPso();
    }

    void Gui::Tick(float time)
    {
        ImGuiIO& io = ImGui::GetIO();
        auto &ext = render->GetSwapchain()->GetExtent();

        io.DisplaySize = ImVec2(static_cast<float>(ext.width), static_cast<float>(ext.height));
        io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
        io.DeltaTime = time;

        ImGui::NewFrame();

        bool open = true;
        ImGui::ShowDemoWindow(&open);

        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();

        if (drawData->TotalVtxCount > 0)
        {
            auto vertexSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
            auto indexSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);
            bool resize = CreateOrResize(res->vb, vertexSize, true);
            resize |= CreateOrResize(res->ib, indexSize, false);
            if (resize) {
                vk::VertexAssembly::Descriptor desc = {};
                res->assembler = std::make_shared<vk::VertexAssembly>(*render->device);
                res->assembler->AddVertexBuffer(res->vb);
                res->assembler->SetIndexBuffer(res->ib);
                res->assembler->SetIndexType(sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
            }

            ImDrawVert* vtx_dst = reinterpret_cast<ImDrawVert*>(res->vb->Map());
            ImDrawIdx* idx_dst = reinterpret_cast<ImDrawIdx*>(res->ib->Map());
            for (int n = 0; n < drawData->CmdListsCount; n++)
            {
                const ImDrawList* cmd_list = drawData->CmdLists[n];
                memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
                memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
                vtx_dst += cmd_list->VtxBuffer.Size;
                idx_dst += cmd_list->IdxBuffer.Size;
            }
            res->vb->UnMap();
            res->ib->UnMap();
        }

        ImVec2 scale;
        scale.x = 2.0f / drawData->DisplaySize.x;
        scale.y = 2.0f / drawData->DisplaySize.y;
        ImVec2 translate;
        translate.x = -1.0f - drawData->DisplayPos.x * scale.x;
        translate.y = -1.0f - drawData->DisplayPos.y * scale.y;
        res->pushConstants->WriteData(scale, 0);
        res->pushConstants->WriteData(translate, sizeof(ImVec2));

        int fb_width = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
        int fb_height = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);

        render->SetEncoder([&](vk::GraphicsEncoder &encoder) {
            encoder.BindPipeline(res->pso);
            encoder.BindShaderResource(res->setBinder);
            encoder.BindAssembly(res->assembler);
            encoder.PushConstant(res->pushConstants);


            // Will project scissor/clipping rectangles into framebuffer space
            ImVec2 clip_off = drawData->DisplayPos;         // (0,0) unless using multi-viewports
            ImVec2 clip_scale = drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

            // Render command lists
            // (Because we merged all buffers into a single one, we maintain our own offset into them)
            int global_vtx_offset = 0;
            int global_idx_offset = 0;
            for (int n = 0; n < drawData->CmdListsCount; n++)
            {
                const ImDrawList* cmd_list = drawData->CmdLists[n];
                for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
                {
                    const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                    // Project scissor/clipping rectangles into framebuffer space
                    ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                    ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                    // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                    if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                    if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                    if (clip_max.x > fb_width) { clip_max.x = (float)fb_width; }
                    if (clip_max.y > fb_height) { clip_max.y = (float)fb_height; }
                    if (clip_max.x < clip_min.x || clip_max.y < clip_min.y)
                        continue;

                    // Apply scissor/clipping rectangle
                    VkRect2D scissor;
                    scissor.offset.x = (int32_t)(clip_min.x);
                    scissor.offset.y = (int32_t)(clip_min.y);
                    scissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
                    scissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);

                    encoder.SetScissor(1, &scissor);
                    encoder.DrawIndexed(rhi::CmdDrawIndexed{
                        pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, static_cast<int32_t>(pcmd->VtxOffset + global_vtx_offset), 0
                    });
                }
                global_idx_offset += cmd_list->IdxBuffer.Size;
                global_vtx_offset += cmd_list->VtxBuffer.Size;
            }
        });

        render->OnTick();
    }

    bool Gui::CreateOrResize(vk::BufferPtr &buffer, size_t size, bool vtx)
    {
        auto usageBit = vtx ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (!buffer || size > buffer->GetSize()) {
            vk::Buffer::VkDescriptor bufferInfo = {};
            bufferInfo.size        = size;
            bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usageBit;
            bufferInfo.memory      = VMA_MEMORY_USAGE_CPU_TO_GPU;
            buffer = render->device->CreateDeviceObject<vk::Buffer>(bufferInfo);
            return true;
        }
        return false;
    }

    vk::ShaderPtr Gui::InitShader(VkShaderStageFlagBits stage, const std::vector<uint32_t> &spv)
    {
        vk::Shader::VkDescriptor desc = {};
        desc.stage = stage;
        desc.spv  = spv.data();
        desc.size = static_cast<uint32_t>(spv.size() * sizeof(uint32_t));
        return render->device->CreateDeviceObject<vk::Shader>(desc);
    }

    void Gui::InitPso()
    {
        vk::DescriptorSetLayout::VkDescriptor desLayoutInfo = {};
        desLayoutInfo.bindings.emplace(0, vk::DescriptorSetLayout::SetBinding{
                                              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                              1,
                                              VK_SHADER_STAGE_FRAGMENT_BIT,
                                          });

        vk::PipelineLayout::VkDescriptor pLayoutInfo = {};
        pLayoutInfo.desLayouts.emplace_back(desLayoutInfo);
        pLayoutInfo.pushConstants.emplace_back(VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, 4 * sizeof(float)});
        res->pipelineLayout = render->device->CreateDeviceObject<vk::PipelineLayout>(pLayoutInfo);

        vk::VertexInput::Descriptor viInfo = {};
        viInfo.bindings.emplace_back(rhi::VertexBindingDesc{0, sizeof(ImDrawVert), rhi::VertexInputRate::PER_VERTEX});

        viInfo.attributes.emplace_back(rhi::VertexAttributeDesc{0, 0, static_cast<uint32_t>(IM_OFFSETOF(ImDrawVert, pos)), rhi::Format::F_RG32});
        viInfo.attributes.emplace_back(rhi::VertexAttributeDesc{1, 0, static_cast<uint32_t>(IM_OFFSETOF(ImDrawVert, uv)), rhi::Format::F_RG32});
        viInfo.attributes.emplace_back(rhi::VertexAttributeDesc{2, 0, static_cast<uint32_t>(IM_OFFSETOF(ImDrawVert, col)), rhi::Format::F_RGBA8});

        res->vertexInput = std::make_shared<vk::VertexInput>();
        res->vertexInput->Init(viInfo);

        vk::GraphicsPipeline::Program pgm;
        pgm.shaders.emplace_back(InitShader(VK_SHADER_STAGE_VERTEX_BIT, VS_SPV));
        pgm.shaders.emplace_back(InitShader(VK_SHADER_STAGE_FRAGMENT_BIT, FS_SPV));

        vk::GraphicsPipeline::State        state;
        state.depthStencil.depthTestEnable = false;
        state.depthStencil.depthWriteEnable = false;
        state.raster.cullMode = VK_CULL_MODE_NONE;

        state.blends.blendStates.emplace_back(vk::GraphicsPipeline::BlendState{
            VK_TRUE,
            VK_BLEND_FACTOR_SRC_ALPHA,
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        });

        vk::GraphicsPipeline::VkDescriptor psoInfo = {};

        psoInfo.program = &pgm;
        psoInfo.state = &state;
        psoInfo.vertexInput    = res->vertexInput;
        psoInfo.renderPass     = render->GetRenderPass();
        psoInfo.pipelineLayout = res->pipelineLayout;
        psoInfo.subPassIndex   = 0;
        res->pso = render->device->CreateDeviceObject<vk::GraphicsPipeline>(psoInfo);
        res->pushConstants = vk::PushConstants::CreateFromPipelineLayout(res->pipelineLayout);
        res->set = res->pipelineLayout->Allocate(render->GetDescriptorSetPool(), 0);
        res->set->CreateWriter()
            .Write(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, res->fontImageView, res->sampler)
            .Update();

        res->setBinder = std::make_shared<vk::DescriptorSetBinder>();
        res->setBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        res->setBinder->SetPipelineLayout(res->pipelineLayout);
        res->setBinder->BindSet(0, res->set);
    }

    void Gui::InitFont()
    {
        ImGuiIO       &io = ImGui::GetIO();
        unsigned char *pixels;
        int            width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        size_t uploadSize = width * height * 4 * sizeof(char);

        vk::Image::VkDescriptor imageDesc = {};

        imageDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageDesc.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
        imageDesc.usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        res->fontImage = render->device->CreateDeviceObject<vk::Image>(imageDesc);
        res->fontImageView = vk::ImageView::CreateImageView(res->fontImage, {VK_IMAGE_VIEW_TYPE_2D, imageDesc.format});
        res->sampler = render->device->CreateDeviceObject<vk::Sampler>(vk::Sampler::VkDescriptor{});

        auto queue = render->device->GetAsyncTransferQueue();
        auto handle = queue->UploadImage(res->fontImage, rhi::ImageUploadRequest{
                                                                                 pixels, 0, uploadSize, 0, 0,
                                                                                 {0, 0, 0}, {imageDesc.extent.width, imageDesc.extent.height, 1}
                                                                             });
        queue->Wait(handle);
        io.Fonts->SetTexID((ImTextureID)(intptr_t)res->fontImage->GetNativeHandle());
    }

} // namespace sky::perf