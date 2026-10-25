//
// Created on 2026/09/21.
//

#include <ui/render/UIRenderer.h>

#include "UIShader.h"

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
#include <aurora/shader/IShaderCompiler.h>
#include <core/logger/Logger.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector4.h>

#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>

static const char *TAG = "UIRenderer";

namespace sky::ui {

    using namespace sky::aurora;

    namespace {

        bool ValidImage(const UIImageData &image)
        {
            if (image.width == 0 || image.height == 0 || image.width > std::numeric_limits<size_t>::max() / 4 / image.height) {
                return false;
            }
            return image.pixels.size() == static_cast<size_t>(image.width) * image.height * 4;
        }

        // UI shader bytecode target must match the active RHI backend.
        ShaderTarget ToShaderTarget(API api)
        {
            switch (api) {
            case API::DX12: return ShaderTarget::DXIL;
            case API::METAL: return ShaderTarget::MSL;
            case API::VULKAN:
            case API::DEFAULT:
            default: return ShaderTarget::SPIRV;
            }
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

        IShaderCompiler *compiler = ShaderCompilerFactory::Get().GetCompiler();
        if (compiler == nullptr) {
            LOG_E(TAG, "UI shader compiler module is not loaded");
            return false;
        }
        auto makeFunction = [&](const char *entry, ShaderStageFlagBit stage) -> ShaderFunction *{
            ShaderCompileDesc compileDesc = {};
            compileDesc.source            = detail::UI_SHADER_SOURCE;
            compileDesc.sourcePath        = "ui/UIShader.slang";
            compileDesc.entry             = entry;
            compileDesc.stage             = stage;
            compileDesc.target            = ToShaderTarget(device->GetAPI());

            ShaderCompileResult result;
            if (!compiler->Compile(compileDesc, result)) {
                LOG_E(TAG, "UI shader compile failed: %s", result.errorInfo.c_str());
                return nullptr;
            }
            ShaderFunction::Descriptor fnDesc = {};
            fnDesc.stage                      = stage;
            fnDesc.entry                      = result.identity.nativeEntry;
            fnDesc.artifact                   = std::make_shared<const ShaderStageArtifact>(std::move(result));
            return device->CreateShaderFunction(fnDesc);
        };

        vs = makeFunction("vs_main", ShaderStageFlagBit::VS);
        ps = makeFunction("fs_main", ShaderStageFlagBit::FS);
        if (vs == nullptr || ps == nullptr) {
            return false;
        }

        std::string error;
        reflection = {};
        if (!MergeShaderReflections(vs->GetArtifact()->reflection, reflection, error) ||
            !MergeShaderReflections(ps->GetArtifact()->reflection, reflection, error)) {
            LOG_E(TAG, "UI shader reflection link failed: %s", error.c_str());
            return false;
        }

        bool sawTex = false;
        bool sawSmp = false;
        for (const auto &res : reflection.resources) {
            sawTex |= res.name == "tex" && res.declarationOwner == "ui/UIShader" && res.set == 3 && res.binding == texBinding &&
                      res.type == ShaderResourceType::SAMPLED_IMAGE && res.count == 1 && !res.descriptorArray;
            sawSmp |= res.name == "smp" && res.declarationOwner == "ui/UIShader" && res.set == 3 && res.binding == smpBinding &&
                      res.type == ShaderResourceType::SAMPLER && res.count == 1 && !res.descriptorArray;
        }
        if (!sawTex || !sawSmp || reflection.resources.size() != 2) {
            LOG_E(TAG, "UI shader requires its declared texture and sampler bindings");
            return false;
        }

        Shader::Descriptor shaderDesc = {};
        shaderDesc.vs                 = vs.Get();
        shaderDesc.ps                 = ps.Get();
        shaderDesc.reflection         = &reflection;
        shader                        = device->CreateShader(shaderDesc);
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
        state.vertexBindings.push_back(VertexBindingDesc{0, static_cast<uint32_t>(sizeof(UIVertex)), VertexInputRate::PER_VERTEX});

        VertexAttributeDesc attrPos   = {};
        attrPos.location              = 0;
        attrPos.binding               = 0;
        attrPos.offset                = static_cast<uint32_t>(offsetof(UIVertex, x));
        attrPos.format                = Format::F_RG32;
        attrPos.semantic              = "POSITION";
        attrPos.semanticIndex         = 0;
        VertexAttributeDesc attrUv    = {};
        attrUv.location               = 1;
        attrUv.binding                = 0;
        attrUv.offset                 = static_cast<uint32_t>(offsetof(UIVertex, u));
        attrUv.format                 = Format::F_RG32;
        attrUv.semantic               = "TEXCOORD";
        attrUv.semanticIndex          = 0;
        VertexAttributeDesc attrColor = {};
        attrColor.location            = 2;
        attrColor.binding             = 0;
        attrColor.offset              = static_cast<uint32_t>(offsetof(UIVertex, color));
        attrColor.format              = Format::F_RGBA8;
        attrColor.semantic            = "COLOR";
        attrColor.semanticIndex       = 0;
        VertexAttributeDesc attrRound = {};
        attrRound.location            = 3;
        attrRound.binding             = 0;
        attrRound.offset              = static_cast<uint32_t>(offsetof(UIVertex, roundCenterX));
        attrRound.format              = Format::F_RGBA32;
        attrRound.semantic            = "TEXCOORD";
        attrRound.semanticIndex       = 1;

        VertexAttributeDesc attrRadius = {};
        attrRadius.location            = 4;
        attrRadius.binding             = 0;
        attrRadius.offset              = static_cast<uint32_t>(offsetof(UIVertex, roundRadius));
        attrRadius.format              = Format::F_R32;
        attrRadius.semantic            = "TEXCOORD";
        attrRadius.semanticIndex       = 2;

        state.vertexAttributes = {attrPos, attrUv, attrColor, attrRound, attrRadius};

        GraphicsPipeline::Descriptor pipeDesc = {};
        pipeDesc.state                        = &state;
        pipeDesc.shader                       = shader.Get();
        pipeDesc.format.colors[0]             = colorFormat;
        pipeDesc.format.numColors             = 1;
        pipeDesc.format.sampleCount           = SampleCount::X1;
        pipeline                              = device->CreatePipelineState(pipeDesc);
        if (pipeline == nullptr) {
            LOG_E(TAG, "UI pipeline creation failed");
            return false;
        }

        // Rounded-box SDF compilation failure retains the flat shape fallback.
        psRound = makeFunction("fs_round", ShaderStageFlagBit::FS);
        if (psRound != nullptr) {
            roundReflection = {};
            if (!MergeShaderReflections(vs->GetArtifact()->reflection, roundReflection, error) ||
                !MergeShaderReflections(psRound->GetArtifact()->reflection, roundReflection, error)) {
                LOG_E(TAG, "UI round shader reflection link failed: %s", error.c_str());
                return false;
            }
            Shader::Descriptor roundShaderDesc = {};
            roundShaderDesc.vs                 = vs.Get();
            roundShaderDesc.ps                 = psRound.Get();
            roundShaderDesc.reflection         = &roundReflection;
            roundShader                        = device->CreateShader(roundShaderDesc);
            if (roundShader != nullptr) {
                GraphicsPipeline::Descriptor roundPipeDesc = {};
                roundPipeDesc.state                        = &state;
                roundPipeDesc.shader                       = roundShader.Get();
                roundPipeDesc.format                       = pipeDesc.format;
                roundPipeline                              = device->CreatePipelineState(roundPipeDesc);
            }
        }

        Sampler::Descriptor samplerDesc = {};
        samplerDesc.magFilter           = Filter::LINEAR;
        samplerDesc.minFilter           = Filter::LINEAR;
        samplerDesc.mipmapMode          = MipFilter::LINEAR;
        samplerDesc.addressModeU        = WrapMode::CLAMP_TO_EDGE;
        samplerDesc.addressModeV        = WrapMode::CLAMP_TO_EDGE;
        samplerDesc.addressModeW        = WrapMode::CLAMP_TO_EDGE;
        sampler                         = device->CreateSampler(samplerDesc);

        // Built-in white 1x1 texture under UI_INVALID_TEXTURE.
        TextureEntry white;
        white.width         = 1;
        white.height        = 1;
        white.pendingPixels = {0xFF, 0xFF, 0xFF, 0xFF};
        textures.emplace(UI_INVALID_TEXTURE, std::move(white));
        return true;
    }

    void UIRenderer::Shutdown()
    {
        textures.clear();
        retiredTextures.clear();
        staging.Reset(nullptr);
        sampler.Reset(nullptr);
        indexBuffer.Reset(nullptr);
        vertexBuffer.Reset(nullptr);
        roundPipeline.Reset(nullptr);
        pipeline.Reset(nullptr);
        roundShader.Reset(nullptr);
        shader.Reset(nullptr);
        psRound.Reset(nullptr);
        ps.Reset(nullptr);
        vs.Reset(nullptr);
        device         = nullptr;
        vertexCapacity = 0;
        indexCapacity  = 0;
        nextTextureId  = 1;
    }

    CounterPtr<ResourceGroup> UIRenderer::CreateTextureGroup(Image *image)
    {
        if (device == nullptr || shader == nullptr || image == nullptr || sampler == nullptr || image->GetDevice() != device) {
            return nullptr;
        }
        ResourceGroup::Descriptor groupDesc = {};
        groupDesc.shader                    = shader.Get();
        groupDesc.set                       = 3;
        CounterPtr<ResourceGroup> group     = device->CreateResourceGroup(groupDesc);
        if (group != nullptr) {
            auto groupEncoder = group->CreateEncoder();
            if (!groupEncoder || !groupEncoder->WriteImage(texBinding, image, ImageLayout::SHADER_READ_ONLY) ||
                !groupEncoder->WriteSampler(smpBinding, sampler.Get()) || !groupEncoder->End()) {
                return nullptr;
            }
        }
        return group;
    }

    ResourceGroup *UIRenderer::FindGroup(UITextureId id) const
    {
        auto it = textures.find(id);
        if (it != textures.end() && it->second.group != nullptr) {
            return it->second.group.Get();
        }
        auto white = textures.find(UI_INVALID_TEXTURE);
        return white == textures.end() ? nullptr : white->second.group.Get();
    }

    UITextureId UIRenderer::RegisterTexture(const UIImageData &image)
    {
        if (device == nullptr || !ValidImage(image)) {
            return UI_INVALID_TEXTURE;
        }
        // Allocate an unused id (skip ids taken by RegisterImage / other pages).
        while (nextTextureId < 0x20000000u && textures.find(nextTextureId) != textures.end()) {
            ++nextTextureId;
        }
        if (nextTextureId >= 0x20000000u) {
            return UI_INVALID_TEXTURE;
        }
        return AddTexture(nextTextureId++, image);
    }

    UITextureId UIRenderer::RegisterTextureAs(UITextureId id, const UIImageData &image)
    {
        if (id == UI_INVALID_TEXTURE || textures.find(id) != textures.end()) {
            return id;
        }
        return AddTexture(id, image);
    }

    UITextureId UIRenderer::AddTexture(UITextureId id, const UIImageData &image)
    {
        if (device == nullptr || !ValidImage(image)) {
            return UI_INVALID_TEXTURE;
        }
        TextureEntry entry;
        entry.width         = image.width;
        entry.height        = image.height;
        entry.pendingPixels = image.pixels;
        textures.emplace(id, std::move(entry));
        return id;
    }

    void UIRenderer::RetireTexture(TextureEntry &&entry)
    {
        // CPU data is not referenced by commands; only GPU generation resources
        // must survive until the next host-provided completion boundary.
        entry.pendingPixels.clear();
        if (entry.image != nullptr || entry.staging != nullptr || entry.group != nullptr) {
            retiredTextures.push_back(std::move(entry));
        }
    }

    void UIRenderer::CollectRetiredTextures()
    {
        retiredTextures.clear();
    }

    bool UIRenderer::PrepareTexture(TextureEntry &entry)
    {
        if (device == nullptr || entry.width == 0 || entry.height == 0 || entry.pendingPixels.empty()) {
            return false;
        }
        TextureEntry next;
        next.width             = entry.width;
        next.height            = entry.height;
        Image::Descriptor desc = {};
        desc.imageType         = ImageType::IMAGE_2D;
        desc.format            = PixelFormat::RGBA8_UNORM;
        desc.extent            = {next.width, next.height, 1};
        desc.mipLevels         = 1;
        desc.arrayLayers       = 1;
        desc.samples           = SampleCount::X1;
        desc.usage             = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
        desc.memory            = MemoryType::GPU_ONLY;
        next.image             = device->CreateImage(desc);

        Buffer::Descriptor stageDesc = {};
        stageDesc.size               = entry.pendingPixels.size();
        stageDesc.usage              = BufferUsageFlagBit::TRANSFER_SRC;
        stageDesc.memory             = MemoryType::CPU_TO_GPU;
        next.staging                 = device->CreateBuffer(stageDesc);
        if (next.image == nullptr || next.staging == nullptr) {
            return false;
        }
        uint8_t *mapped = next.staging->Map();
        if (mapped == nullptr) {
            return false;
        }
        std::memcpy(mapped, entry.pendingPixels.data(), entry.pendingPixels.size());
        next.staging->UnMap();
        next.group = CreateTextureGroup(next.image.Get());
        if (next.group == nullptr) {
            return false;
        }
        next.pendingPixels = std::move(entry.pendingPixels);
        RetireTexture(std::move(entry));
        entry = std::move(next);
        return true;
    }

    void UIRenderer::UpdateTexture(UITextureId id, const UIImageData &image)
    {
        auto it = textures.find(id);
        if (it == textures.end() || !ValidImage(image)) {
            return;
        }
        TextureEntry &entry = it->second;
        entry.width         = image.width;
        entry.height        = image.height;
        entry.pendingPixels = image.pixels;
        entry.uploaded      = false;
        // Do not map or release the previous generation here: an attached
        // renderer may still be referenced by the preceding/current frame.
    }

    void UIRenderer::ReleaseTexture(UITextureId id)
    {
        if (id == UI_INVALID_TEXTURE) {
            return;
        }
        const auto it = textures.find(id);
        if (it != textures.end()) {
            RetireTexture(std::move(it->second));
            textures.erase(it);
        }
    }

    bool UIRenderer::RegisterImage(UITextureId id, Image *image)
    {
        if (device == nullptr || image == nullptr) {
            return false;
        }
        TextureEntry entry;
        entry.image    = CounterPtr<Image>(image);
        entry.uploaded = true; // GPU-provided; no CPU upload
        entry.group    = CreateTextureGroup(image);
        if (entry.group == nullptr) {
            return false;
        }
        const auto existing = textures.find(id);
        if (existing != textures.end()) {
            RetireTexture(std::move(existing->second));
            existing->second = std::move(entry);
        } else {
            textures.emplace(id, std::move(entry));
        }
        return true;
    }

    bool UIRenderer::EnsureBuffers(uint64_t vertexBytes, uint64_t indexBytes)
    {
        if (device == nullptr) {
            return false;
        }
        if (vertexBuffer == nullptr || vertexBytes > vertexCapacity) {
            vertexCapacity          = vertexBytes == 0 ? 256 : vertexBytes;
            Buffer::Descriptor desc = {};
            desc.size               = vertexCapacity;
            desc.usage              = BufferUsageFlagBit::VERTEX;
            desc.memory             = MemoryType::CPU_TO_GPU;
            vertexBuffer            = device->CreateBuffer(desc);
        }
        if (indexBuffer == nullptr || indexBytes > indexCapacity) {
            indexCapacity           = indexBytes == 0 ? 64 : indexBytes;
            Buffer::Descriptor desc = {};
            desc.size               = indexCapacity;
            desc.usage              = BufferUsageFlagBit::INDEX;
            desc.memory             = MemoryType::CPU_TO_GPU;
            indexBuffer             = device->CreateBuffer(desc);
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
                // Pixel y grows down (origin top). The clip-space Y axis differs per
                // backend: Vulkan NDC has +Y down, D3D12/Metal have +Y up. Pick the
                // sign from the device so pixel y=0 lands at the framebuffer top on
                // every backend.
                const float   w     = static_cast<float>(surfaceWidth > 0 ? surfaceWidth : 1);
                const float   h     = static_cast<float>(surfaceHeight > 0 ? surfaceHeight : 1);
                const float   ySign = device->GetCapability().clipSpaceYDown ? 1.0f : -1.0f;
                const Matrix4 projection(Vector4(2.0f / w, 0.0f, 0.0f, 0.0f), Vector4(0.0f, ySign * 2.0f / h, 0.0f, 0.0f),
                                         Vector4(0.0f, 0.0f, 1.0f, 0.0f), Vector4(-1.0f, -ySign, 0.0f, 1.0f));

                auto *dst = reinterpret_cast<UIVertex *>(mapped);
                for (size_t i = 0; i < drawData.vertices.size(); ++i) {
                    const UIVertex &src  = drawData.vertices[i];
                    const Vector4   clip = projection * Vector4(src.x, src.y, 0.0f, 1.0f);
                    UIVertex       &out  = dst[i];
                    out                  = src;
                    out.x                = clip.x;
                    out.y                = clip.y;
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
            if (entry.uploaded || entry.pendingPixels.empty() || !PrepareTexture(entry)) {
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
                auto            blit = commandBuffer->CreateBlitEncoder();
                BufferImageCopy copy = {};
                copy.subRange        = ImageSubRangeLayers{0, 0, 1, AspectFlags(AspectFlagBit::COLOR_BIT)};
                copy.imageOffset     = {0, 0, 0};
                copy.imageExtent     = {entry.width, entry.height, 1};
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

            entry.uploaded = true;
            entry.pendingPixels.clear();
            entry.pendingPixels.shrink_to_fit();
        }
    }

    GraphicsEncoder *UIRenderer::BindPipeline(GraphicsEncoder *encoder, uint32_t surfaceWidth, uint32_t surfaceHeight) const
    {
        encoder->BindPipeline(pipeline.Get());
        Viewport vp = {0.f, 0.f, static_cast<float>(surfaceWidth), static_cast<float>(surfaceHeight), 0.f, 1.f};
        encoder->SetViewport(1, &vp);

        BufferView vertexView = {};
        vertexView.buffer     = vertexBuffer.Get();
        vertexView.offset     = 0;
        vertexView.range      = vertexBuffer->GetSize();
        encoder->BindVertexBuffers(0, 1, &vertexView);
        encoder->BindIndexBuffer(indexBuffer.Get(), 0, IndexType::U32);
        return encoder;
    }

    void UIRenderer::Render(GraphicsEncoder *encoder, const UIDrawData &drawData, uint32_t surfaceWidth, uint32_t surfaceHeight)
    {
        if (encoder == nullptr || pipeline == nullptr || vertexBuffer == nullptr || indexBuffer == nullptr) {
            return;
        }
        BindPipeline(encoder, surfaceWidth, surfaceHeight);

        const GraphicsPipeline *boundPipeline   = pipeline.Get();
        UITextureId             boundTexture    = UI_INVALID_TEXTURE;
        bool                    hasBoundTexture = false;
        for (const auto &command : drawData.commands) {
            const bool              useRound     = command.shape && roundPipeline != nullptr;
            GraphicsPipeline *const wantPipeline = useRound ? roundPipeline.Get() : pipeline.Get();
            if (wantPipeline != boundPipeline) {
                encoder->BindPipeline(wantPipeline);
                boundPipeline   = wantPipeline;
                hasBoundTexture = false;
            }
            if (!useRound && (!hasBoundTexture || command.textureId != boundTexture)) {
                ResourceGroup *const group = FindGroup(command.textureId);
                if (group == nullptr) {
                    hasBoundTexture = false;
                    LOG_W(TAG, "UI draw has no available texture or white fallback");
                    continue;
                }
                encoder->BindResourceGroup(3, group);
                boundTexture    = command.textureId;
                hasBoundTexture = true;
            }

            Rect2D scissor = {{static_cast<int32_t>(command.clip.left), static_cast<int32_t>(command.clip.top)},
                              {static_cast<uint32_t>(command.clip.Width()), static_cast<uint32_t>(command.clip.Height())}};
            encoder->SetScissor(1, &scissor);

            CmdDrawIndexed drawCmd = {};
            drawCmd.indexCount     = command.indexCount;
            drawCmd.instanceCount  = 1;
            drawCmd.firstIndex     = command.indexOffset;
            encoder->DrawIndexed(drawCmd);
        }
    }

} // namespace sky::ui
