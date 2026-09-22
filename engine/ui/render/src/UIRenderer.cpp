//
// Created on 2026/09/21.
//

#include <ui/render/UIRenderer.h>

#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/DescriptorEncoder.h>
#include <aurora/rhi/Device.h>
#include <aurora/rhi/Encoder.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/PipelineState.h>
#include <aurora/rhi/ResourceGroup.h>
#include <aurora/rhi/Sampler.h>
#include <aurora/rhi/Shader.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <core/archive/BinaryData.h>
#include <core/logger/Logger.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector4.h>

#include <cstddef>
#include <cstring>

static const char *TAG = "UIRenderer";

namespace sky::ui {

    using namespace sky::aurora;

    namespace {

        const char *kUiShader = R"(
[[vk::binding(0, 0)]] Texture2D tex;
[[vk::binding(1, 0)]] SamplerState smp;
struct VSIn { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 color : COLOR0; };
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; float4 color : COLOR0; };
[shader("vertex")]
VSOut vs_main(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos, 0.0, 1.0);
    o.uv = i.uv;
    o.color = i.color;
    return o;
}
[shader("fragment")]
float4 fs_main(VSOut i) : SV_Target {
    return i.color * tex.Sample(smp, i.uv);
}
)";

        // Whole-program reflection: set 0 = { Tex (sampled image), Smp (sampler) }.
        ShaderReflection MakeReflection()
        {
            ShaderReflection refl{};
            ShaderResource tex{};
            tex.name    = "tex";
            tex.set     = 0;
            tex.binding = 0;
            tex.type    = ShaderResourceType::SAMPLED_IMAGE;
            tex.count   = 1;
            refl.resources.push_back(tex);

            ShaderResource smp{};
            smp.name    = "smp";
            smp.set     = 0;
            smp.binding = 1;
            smp.type    = ShaderResourceType::SAMPLER;
            smp.count   = 1;
            refl.resources.push_back(smp);
            return refl;
        }

        const ShaderReflection &GetReflection()
        {
            static const ShaderReflection reflection = MakeReflection();
            return reflection;
        }

    } // namespace

    UIRenderer::~UIRenderer()
    {
        Shutdown();
    }

    bool UIRenderer::Init(Device *inDevice, PixelFormat inColorFormat)
    {
        device      = inDevice;
        colorFormat = inColorFormat;
        if (device == nullptr) {
            return false;
        }

        ShaderCompilerSlang compiler;
        auto makeFunction = [&](const char *entry, ShaderStageFlagBit stage) -> ShaderFunction * {
            ShaderCompileDesc compileDesc = {};
            compileDesc.source = kUiShader;
            compileDesc.entry  = entry;
            compileDesc.stage  = stage;
            compileDesc.target = ShaderTarget::SPIRV;

            ShaderCompileResult result;
            if (!compiler.Compile(compileDesc, result)) {
                LOG_E(TAG, "UI shader compile failed: %s", result.errorInfo.c_str());
                return nullptr;
            }
            const uint32_t bytes = static_cast<uint32_t>(result.data.size() * sizeof(uint32_t));
            auto binary = CounterPtr<BinaryData>(new BinaryData(bytes));
            std::memcpy(binary->Data(), result.data.data(), bytes);

            auto *provider = new ShaderBinaryProvider();
            provider->binaryData = binary;

            ShaderFunction::Descriptor fnDesc = {};
            fnDesc.stage = stage;
            fnDesc.data  = CounterPtr<ShaderDataProvider>(provider);
            fnDesc.entry = entry;
            return device->CreateShaderFunction(fnDesc);
        };

        vs = makeFunction("vs_main", ShaderStageFlagBit::VS);
        ps = makeFunction("fs_main", ShaderStageFlagBit::FS);
        if (vs == nullptr || ps == nullptr) {
            return false;
        }

        Shader::Descriptor shaderDesc = {};
        shaderDesc.vs         = vs.Get();
        shaderDesc.ps         = ps.Get();
        shaderDesc.reflection = &GetReflection();
        shader = device->CreateShader(shaderDesc);
        if (shader == nullptr) {
            LOG_E(TAG, "UI shader creation failed");
            return false;
        }

        PipelineState state = {};
        state.blendStates.resize(1);
        state.blendStates[0].blendEn      = true;
        state.blendStates[0].srcColor     = BlendFactor::SRC_ALPHA;
        state.blendStates[0].dstColor     = BlendFactor::ONE_MINUS_SRC_ALPHA;
        state.blendStates[0].srcAlpha     = BlendFactor::ONE;
        state.blendStates[0].dstAlpha     = BlendFactor::ONE_MINUS_SRC_ALPHA;
        state.blendStates[0].colorBlendOp = BlendOp::ADD;
        state.blendStates[0].alphaBlendOp = BlendOp::ADD;
        state.vertexBindings.push_back(
            VertexBindingDesc{0, static_cast<uint32_t>(sizeof(UIVertex)), VertexInputRate::PER_VERTEX});

        VertexAttributeDesc attrPos = {};
        attrPos.location = 0;
        attrPos.binding  = 0;
        attrPos.offset   = static_cast<uint32_t>(offsetof(UIVertex, x));
        attrPos.format   = Format::F_RG32;
        VertexAttributeDesc attrUv = {};
        attrUv.location = 1;
        attrUv.binding  = 0;
        attrUv.offset   = static_cast<uint32_t>(offsetof(UIVertex, u));
        attrUv.format   = Format::F_RG32;
        VertexAttributeDesc attrColor = {};
        attrColor.location = 2;
        attrColor.binding  = 0;
        attrColor.offset   = static_cast<uint32_t>(offsetof(UIVertex, color));
        attrColor.format   = Format::F_RGBA8;
        state.vertexAttributes = {attrPos, attrUv, attrColor};

        GraphicsPipeline::Descriptor pipeDesc = {};
        pipeDesc.state              = &state;
        pipeDesc.shader             = shader.Get();
        pipeDesc.format.colors[0]   = colorFormat;
        pipeDesc.format.numColors   = 1;
        pipeDesc.format.sampleCount = SampleCount::X1;
        pipeline = device->CreatePipelineState(pipeDesc);
        if (pipeline == nullptr) {
            LOG_E(TAG, "UI pipeline creation failed");
            return false;
        }

        Sampler::Descriptor samplerDesc = {};
        samplerDesc.magFilter    = Filter::LINEAR;
        samplerDesc.minFilter    = Filter::LINEAR;
        samplerDesc.mipmapMode   = MipFilter::LINEAR;
        samplerDesc.addressModeU = WrapMode::CLAMP_TO_EDGE;
        samplerDesc.addressModeV = WrapMode::CLAMP_TO_EDGE;
        samplerDesc.addressModeW = WrapMode::CLAMP_TO_EDGE;
        sampler = device->CreateSampler(samplerDesc);

        // Built-in white 1x1 texture under UI_INVALID_TEXTURE.
        TextureEntry white;
        white.width  = 1;
        white.height = 1;
        white.pendingPixels = {0xFF, 0xFF, 0xFF, 0xFF};
        Image::Descriptor whiteDesc = {};
        whiteDesc.imageType   = ImageType::IMAGE_2D;
        whiteDesc.format      = PixelFormat::RGBA8_UNORM;
        whiteDesc.extent      = {1, 1, 1};
        whiteDesc.mipLevels   = 1;
        whiteDesc.arrayLayers = 1;
        whiteDesc.samples     = SampleCount::X1;
        whiteDesc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
        whiteDesc.memory      = MemoryType::GPU_ONLY;
        white.image = device->CreateImage(whiteDesc);

        Buffer::Descriptor stageDesc = {};
        stageDesc.size   = 4;
        stageDesc.usage  = BufferUsageFlagBit::TRANSFER_SRC;
        stageDesc.memory = MemoryType::CPU_TO_GPU;
        white.staging = device->CreateBuffer(stageDesc);
        if (white.staging != nullptr) {
            if (uint8_t *mapped = white.staging->Map()) {
                mapped[0] = 0xFF;
                mapped[1] = 0xFF;
                mapped[2] = 0xFF;
                mapped[3] = 0xFF;
            }
            white.staging->UnMap();
        }
        white.group = CounterPtr<ResourceGroup>(CreateTextureGroup(white.image.Get()));
        textures.emplace(UI_INVALID_TEXTURE, std::move(white));
        return true;
    }

    void UIRenderer::Shutdown()
    {
        textures.clear();
        staging.Reset(nullptr);
        sampler.Reset(nullptr);
        indexBuffer.Reset(nullptr);
        vertexBuffer.Reset(nullptr);
        pipeline.Reset(nullptr);
        shader.Reset(nullptr);
        ps.Reset(nullptr);
        vs.Reset(nullptr);
        device          = nullptr;
        vertexCapacity  = 0;
        indexCapacity   = 0;
        nextTextureId   = 1;
    }

    ResourceGroup *UIRenderer::CreateTextureGroup(Image *image)
    {
        if (device == nullptr || shader == nullptr || image == nullptr) {
            return nullptr;
        }
        ResourceGroup::Descriptor groupDesc = {};
        groupDesc.shader = shader.Get();
        groupDesc.set    = 0;
        ResourceGroup *group = device->CreateResourceGroup(groupDesc);
        if (group != nullptr) {
            auto groupEncoder = group->CreateEncoder();
            groupEncoder->WriteImage(0, image, ImageLayout::SHADER_READ_ONLY);
            groupEncoder->WriteSampler(1, sampler.Get());
            groupEncoder->End();
        }
        return group;
    }

    ResourceGroup *UIRenderer::FindGroup(UITextureId id) const
    {
        auto it = textures.find(id);
        if (it != textures.end()) {
            return it->second.group.Get();
        }
        auto white = textures.find(UI_INVALID_TEXTURE);
        return white == textures.end() ? nullptr : white->second.group.Get();
    }

    UITextureId UIRenderer::RegisterTexture(const UIImageData &image)
    {
        if (device == nullptr || image.width == 0 || image.height == 0) {
            return UI_INVALID_TEXTURE;
        }
        // Allocate an unused id (skip ids taken by RegisterImage / other pages).
        while (textures.find(nextTextureId) != textures.end()) {
            ++nextTextureId;
        }
        const UITextureId id = nextTextureId++;

        TextureEntry entry;
        entry.width  = image.width;
        entry.height = image.height;
        entry.pendingPixels = image.pixels;

        Image::Descriptor desc = {};
        desc.imageType   = ImageType::IMAGE_2D;
        desc.format      = PixelFormat::RGBA8_UNORM;
        desc.extent      = {image.width, image.height, 1};
        desc.mipLevels   = 1;
        desc.arrayLayers = 1;
        desc.samples     = SampleCount::X1;
        desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
        desc.memory      = MemoryType::GPU_ONLY;
        entry.image = device->CreateImage(desc);

        Buffer::Descriptor stageDesc = {};
        stageDesc.size   = entry.pendingPixels.size();
        stageDesc.usage  = BufferUsageFlagBit::TRANSFER_SRC;
        stageDesc.memory = MemoryType::CPU_TO_GPU;
        entry.staging = device->CreateBuffer(stageDesc);
        if (entry.staging != nullptr && !entry.pendingPixels.empty()) {
            if (uint8_t *mapped = entry.staging->Map()) {
                std::memcpy(mapped, entry.pendingPixels.data(), entry.pendingPixels.size());
            }
            entry.staging->UnMap();
        }
        entry.group = CounterPtr<ResourceGroup>(CreateTextureGroup(entry.image.Get()));

        textures[id] = std::move(entry);
        return id;
    }

    void UIRenderer::UpdateTexture(UITextureId id, const UIImageData &image)
    {
        auto it = textures.find(id);
        if (it == textures.end()) {
            return;
        }
        TextureEntry &entry = it->second;
        entry.width         = image.width;
        entry.height        = image.height;
        entry.pendingPixels = image.pixels;
        entry.uploaded      = false;
        if (entry.staging != nullptr && !entry.pendingPixels.empty()) {
            if (uint8_t *mapped = entry.staging->Map()) {
                std::memcpy(mapped, entry.pendingPixels.data(), entry.pendingPixels.size());
            }
            entry.staging->UnMap();
        }
    }

    void UIRenderer::ReleaseTexture(UITextureId id)
    {
        if (id == UI_INVALID_TEXTURE) {
            return;
        }
        textures.erase(id);
    }

    bool UIRenderer::RegisterImage(UITextureId id, Image *image)
    {
        if (device == nullptr || image == nullptr) {
            return false;
        }
        TextureEntry entry;
        entry.image    = CounterPtr<Image>(image);
        entry.uploaded = true; // GPU-provided; no CPU upload
        entry.group    = CounterPtr<ResourceGroup>(CreateTextureGroup(image));
        if (entry.group == nullptr) {
            return false;
        }
        textures[id] = std::move(entry);
        return true;
    }

    bool UIRenderer::EnsureBuffers(uint64_t vertexBytes, uint64_t indexBytes)
    {
        if (device == nullptr) {
            return false;
        }
        if (vertexBuffer == nullptr || vertexBytes > vertexCapacity) {
            vertexCapacity = vertexBytes == 0 ? 256 : vertexBytes;
            Buffer::Descriptor desc = {};
            desc.size   = vertexCapacity;
            desc.usage  = BufferUsageFlagBit::VERTEX;
            desc.memory = MemoryType::CPU_TO_GPU;
            vertexBuffer = device->CreateBuffer(desc);
        }
        if (indexBuffer == nullptr || indexBytes > indexCapacity) {
            indexCapacity = indexBytes == 0 ? 64 : indexBytes;
            Buffer::Descriptor desc = {};
            desc.size   = indexCapacity;
            desc.usage  = BufferUsageFlagBit::INDEX;
            desc.memory = MemoryType::CPU_TO_GPU;
            indexBuffer = device->CreateBuffer(desc);
        }
        return vertexBuffer != nullptr && indexBuffer != nullptr;
    }

    void UIRenderer::UpdateDrawData(const UIDrawData &drawData, uint32_t surfaceWidth, uint32_t surfaceHeight)
    {
        const uint64_t vertexBytes = drawData.vertices.size() * sizeof(UIVertex);
        const uint64_t indexBytes  = drawData.indices.size() * sizeof(uint32_t);
        if (!EnsureBuffers(vertexBytes, indexBytes)) {
            return;
        }
        if (vertexBytes > 0) {
            if (uint8_t *mapped = vertexBuffer->Map()) {
                // UI projection matrix: pixel space -> clip space (orthographic).
                // NOTE: Matrix4::operator*(Vector4) = x*row0 + y*row1 + z*row2 + w*row3,
                // so the translation lives in the 4th ROW, not the 4th column.
                // y_clip = 2y/h - 1 maps pixel y=0 to clip y=-1 (framebuffer top).
                const float w = static_cast<float>(surfaceWidth > 0 ? surfaceWidth : 1);
                const float h = static_cast<float>(surfaceHeight > 0 ? surfaceHeight : 1);
                const Matrix4 projection(Vector4(2.0f / w, 0.0f, 0.0f, 0.0f),
                                         Vector4(0.0f, 2.0f / h, 0.0f, 0.0f),
                                         Vector4(0.0f, 0.0f, 1.0f, 0.0f),
                                         Vector4(-1.0f, -1.0f, 0.0f, 1.0f));

                auto *dst = reinterpret_cast<UIVertex *>(mapped);
                for (size_t i = 0; i < drawData.vertices.size(); ++i) {
                    const UIVertex &src = drawData.vertices[i];
                    const Vector4   clip = projection * Vector4(src.x, src.y, 0.0f, 1.0f);
                    UIVertex       &out  = dst[i];
                    out       = src;
                    out.x     = clip.x;
                    out.y     = clip.y;
                }
                vertexBuffer->UnMap();
            }
        }
        if (indexBytes > 0) {
            if (uint8_t *mapped = indexBuffer->Map()) {
                std::memcpy(mapped, drawData.indices.data(), indexBytes);
                indexBuffer->UnMap();
            }
        }
    }

    void UIRenderer::EnsureTextureReady(CommandBuffer *commandBuffer)
    {
        if (commandBuffer == nullptr) {
            return;
        }
        for (auto &[id, entry] : textures) {
            if (entry.uploaded || entry.image == nullptr || entry.staging == nullptr ||
                entry.pendingPixels.empty()) {
                continue;
            }

            BarrierInfo toTransfer{};
            toTransfer.srcStage = PipelineStageBit::TOP;
            toTransfer.dstStage = PipelineStageBit::TRANSFER;
            ImageBarrierInfo uploadBarrier{};
            uploadBarrier.image     = entry.image.Get();
            uploadBarrier.srcAccess = AccessFlagBit::NONE;
            uploadBarrier.dstAccess = AccessFlagBit::COPY_DST;
            uploadBarrier.oldLayout = ImageLayout::UNDEFINED;
            uploadBarrier.newLayout = ImageLayout::TRANSFER_DST;
            toTransfer.imageBarriers.push_back(uploadBarrier);
            commandBuffer->PipelineBarrier(toTransfer);

            {
                auto blit = commandBuffer->CreateBlitEncoder();
                BufferImageCopy copy = {};
                copy.subRange    = ImageSubRangeLayers{0, 0, 1, AspectFlags(AspectFlagBit::COLOR_BIT)};
                copy.imageOffset = {0, 0, 0};
                copy.imageExtent = {entry.width, entry.height, 1};
                blit->CopyBufferToImage(entry.staging.Get(), entry.image.Get(), {copy});
            }

            BarrierInfo toShaderRead{};
            toShaderRead.srcStage = PipelineStageBit::TRANSFER;
            toShaderRead.dstStage = PipelineStageBit::FRAGMENT_SHADER;
            ImageBarrierInfo readBarrier{};
            readBarrier.image     = entry.image.Get();
            readBarrier.srcAccess = AccessFlagBit::COPY_DST;
            readBarrier.dstAccess = AccessFlagBit::SRV;
            readBarrier.oldLayout = ImageLayout::TRANSFER_DST;
            readBarrier.newLayout = ImageLayout::SHADER_READ_ONLY;
            toShaderRead.imageBarriers.push_back(readBarrier);
            commandBuffer->PipelineBarrier(toShaderRead);

            entry.uploaded      = true;
            entry.pendingPixels.clear();
            entry.pendingPixels.shrink_to_fit();
        }
    }

    GraphicsEncoder *UIRenderer::BindPipeline(GraphicsEncoder *encoder, uint32_t surfaceWidth,
                                              uint32_t surfaceHeight) const
    {
        encoder->BindPipeline(pipeline.Get());
        Viewport vp = {0.f, 0.f, static_cast<float>(surfaceWidth), static_cast<float>(surfaceHeight), 0.f, 1.f};
        encoder->SetViewport(1, &vp);

        BufferView vertexView = {};
        vertexView.buffer = vertexBuffer.Get();
        vertexView.offset = 0;
        vertexView.range  = vertexBuffer->GetSize();
        encoder->BindVertexBuffers(0, 1, &vertexView);
        encoder->BindIndexBuffer(indexBuffer.Get(), 0, IndexType::U32);
        return encoder;
    }

    void UIRenderer::Render(GraphicsEncoder *encoder, const UIDrawData &drawData, uint32_t surfaceWidth,
                            uint32_t surfaceHeight)
    {
        if (encoder == nullptr || pipeline == nullptr || vertexBuffer == nullptr || indexBuffer == nullptr) {
            return;
        }
        BindPipeline(encoder, surfaceWidth, surfaceHeight);

        UITextureId boundTexture = static_cast<UITextureId>(~0u);
        for (const auto &command : drawData.commands) {
            if (command.textureId != boundTexture) {
                if (ResourceGroup *group = FindGroup(command.textureId)) {
                    encoder->BindResourceGroup(0, group);
                }
                boundTexture = command.textureId;
            }

            Rect2D scissor = {
                {static_cast<int32_t>(command.clip.left), static_cast<int32_t>(command.clip.top)},
                {static_cast<uint32_t>(command.clip.Width()), static_cast<uint32_t>(command.clip.Height())}};
            encoder->SetScissor(1, &scissor);

            CmdDrawIndexed drawCmd = {};
            drawCmd.indexCount    = command.indexCount;
            drawCmd.instanceCount = 1;
            drawCmd.firstIndex    = command.indexOffset;
            encoder->DrawIndexed(drawCmd);
        }
    }

} // namespace sky::ui
