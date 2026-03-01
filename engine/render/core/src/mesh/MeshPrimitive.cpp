//
// Created by Zach Lee on 2023/8/19.
//

#include <render/mesh/MeshPrimitive.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <render/resource/Buffer.h>
#include <render/resource/SkeletonMesh.h>

namespace sky {
    bool RenderBatch::UpdateBatchGroupLayout() noexcept
    {
        static const Name ShadingBufferName("shading");
        static const Name SkinBufferName("skin");

        bool needUpdateBatchGroup = false;

        layout = techInst.program->RequestLayout(BATCH_SET);

        if (layout && (layout->GetHash() != batchLayoutHash)) {
            batchLayoutHash = layout->GetHash();
            batchGroup = new ResourceGroup();
            batchGroup->Init(layout, *Renderer::Get()->GetMaterialManager()->GetPool());

            const auto *bufferHandler = layout->GetBindingByeName(ShadingBufferName);
            if (bufferHandler != nullptr && (!shadingBuffer || shadingBuffer->GetSize() < bufferHandler->size)) {
                shadingBuffer = new DynamicUniformBuffer();
                shadingBuffer->Init(bufferHandler->size);
                valueVersion = ~(0U);
            }

            batchGroup->BindDynamicUBO(ShadingBufferName, shadingBuffer, 0);
            if (boneBuffer != nullptr) {
                batchGroup->BindDynamicUBO(SkinBufferName, RDDynamicUniformBufferPtr(boneBuffer), 0);
            }

            batchVersion = ~(0U);
            needUpdateBatchGroup = true;
        }

        return needUpdateBatchGroup;
    }

    void RenderBatch::UpdateBatchOptions() noexcept
    {
        techInst.technique->ForEachOption([this](const Name& name) {
            const auto *val = material->GetValue<uint32_t>(name);
            if (val != nullptr) {
                techInst.SetOption(name, static_cast<uint8_t>(*val));
            }
        });
    }

    void RenderBatch::UpdateShadingBuffer() noexcept
    {
        const auto &bufferHandles = layout->GetBufferNameHandles();
        auto *ptr = shadingBuffer->GetAddress();
        for (const auto &[name, handle] : bufferHandles) {
            uint8_t *dst = ptr + handle.offset;
            material->GetValueRaw(name, dst, handle.size);
        }
        shadingBuffer->Upload();
    }

    void RenderBatch::UpdateBatchGroup() noexcept
    {
        const auto &bindingHandlers = layout->GetBindingHandlers();
        const auto &rhiBindings = layout->GetRHILayout()->GetBindings();
        for (const auto &[name, handler] : bindingHandlers) {

            auto bIter = std::find_if(rhiBindings.begin(), rhiBindings.end(),
                [binding = handler.binding](const auto &val) {
                    return binding == val.binding;
            });

            if (bIter != rhiBindings.end()) {
                if (bIter->type == rhi::DescriptorType::COMBINED_IMAGE_SAMPLER || bIter->type == rhi::DescriptorType::SAMPLED_IMAGE) {
                    auto tex = material->GetTexture(name);
                    if (tex) {
                        batchGroup->BindTexture(name, tex->GetImageView(), 0);
                    }
                } else if (bIter->type == rhi::DescriptorType::SAMPLER) {
                    const auto *val = material->GetValue<TextureSampler>(name);
                    if (val != nullptr) {
                        rhi::Sampler::Descriptor desc = {};
                        desc.magFilter = static_cast<rhi::Filter>(val->magFilter);
                        desc.minFilter = static_cast<rhi::Filter>(val->minFilter);
                        desc.mipmapMode = static_cast<rhi::MipFilter>(val->mipMode);
                        desc.addressModeU = static_cast<rhi::WrapMode>(val->addressModeU);
                        desc.addressModeV = static_cast<rhi::WrapMode>(val->addressModeV);
                        desc.addressModeW = static_cast<rhi::WrapMode>(val->addressModeW);
                        desc.maxLod = 13;

                        batchGroup->BindSampler(name, RHI::Get()->GetDevice()->CreateSampler(desc), 0);
                    }
                }
            }
        }
        batchGroup->Update();
    }

    void RenderBatch::Update(const ShaderVariantKey& pipelineKey) noexcept
    {
        bool valueChanged = valueVersion != material->GetValueVersion();

        // update batch options first for batch key
        if (valueChanged) {
            UpdateBatchOptions();
        }

        bool needUpdateBatchGroup = false;

        // update program and layout
        if (techInst.UpdateProgram(pipelineKey)) {
            psoKey = ~(0U);
            needUpdateBatchGroup = UpdateBatchGroupLayout();
        }

        // no batching resource group
        if (!layout) {
            return;
        }

        if (valueChanged) {
            UpdateShadingBuffer();
            valueVersion = material->GetValueVersion();
        }

        needUpdateBatchGroup |= batchVersion != material->GetBatchVersion();
        if (needUpdateBatchGroup) {
            UpdateBatchGroup();
            batchVersion = material->GetBatchVersion();
        }
    }

    uint32_t RenderBatch::CalculatePipelineHash(uint32_t passHash) const noexcept
    {
        uint32_t hash = 0;

        HashCombine32(hash, techInst.program->GetHash());
        HashCombine32(hash, passHash);
        return hash;
    }

    bool RenderBatch::IsReadyToGather() const noexcept
    {
        return !!pso;
    }

    void RenderBatch::BuildPipelineState(const RenderBatchPrepareInfo& info) noexcept
    {
        pso = GraphicsTechnique::BuildPso(techInst.program,
            techInst.technique->GetPipelineState(),
            geometry->Request(techInst.program),
            info.pass, info.subPassId);
    }

    void RenderBatch::Prepare(const RenderBatchPrepareInfo& info) noexcept
    {
        Update(info.pipelineKey);

        uint32_t currentKey = CalculatePipelineHash(info.pass->GetCompatibleHashWithSubPass(info.subPassId));
        if (currentKey != psoKey) {
            BuildPipelineState(info);
            psoKey = currentKey;
        }
    }

    const Name& RenderBatch::GetRasterID() const noexcept
    {
        return techInst.technique->GetRasterID();
    }

    bool RenderSection::PrepareBatch(const RenderBatchPrepareInfo& info) noexcept
    {
        bool needGather = false;
        for (auto& batch : batches) {
            if (batch->GetRasterID() == info.techId) {
                batch->Prepare(info);
                needGather = batch->IsReadyToGather();
            }
        }
        return needGather;
    }

    RenderMaterialPrimitive::RenderMaterialPrimitive(const RDMeshPtr& mesh)
    {
        geometry = mesh->GetGeometry();

        const auto& subMeshes = mesh->GetSubMeshes();

        auto numSection = subMeshes.size();
        materials.resize(numSection);
        sections.resize(numSection);

        for (uint32_t i = 0; i < subMeshes.size(); ++i) {
            materials[i] = mesh->GetMaterialBySubMesh(i);

            const auto& sub = subMeshes[i];
            auto& section = sections[i];

            section.args = rhi::CmdDrawIndexed {
                sub.indexCount,
                1,
                sub.firstIndex,
                static_cast<int32_t>(sub.firstVertex),
                0
            };

            auto* subMat = materials[i].Get();

            auto &techniques = subMat->GetMaterial()->GetGfxTechniques();
            sections[i].batches.resize(techniques.size());
            for (uint32_t j = 0; j < techniques.size(); ++j) {
                auto& tech = techniques[j];
                sections[i].batches[j] = std::make_unique<RenderBatch>(tech);
                sections[i].batches[j]->geometry = geometry.Get();
                sections[i].batches[j]->material = subMat;
            }
        }
    }

    bool RenderMaterialPrimitive::IsReady() const noexcept
    {
        bool ready = true;

        for (auto& mat : materials) {
            ready &= (mat && mat->IsReady());
        }
        return ready;
    }

    void RenderMaterialPrimitive::UpdateWorldBounds(const Matrix4& localToWorld) noexcept
    {
        worldBounds = BoundingBoxSphere::Transform(geometry->localBounds, localToWorld);
    }

    bool RenderMaterialPrimitive::PrepareBatch(const RenderBatchPrepareInfo& info) noexcept
    {
        bool needGather = false;
        for (auto& section : sections) {
            needGather |= section.PrepareBatch(info);
        }

        if (needGather) {
            geometry->Upload();
        }

        return needGather;
    }

    void RenderMaterialPrimitive::GatherRenderItem(IRenderItemGatherContext* context) noexcept
    {
        for (const auto& section : sections) {
            for (auto& batch : section.batches) {
                if (batch->GetRasterID() == context->rasterID) {
                    context->Append(RenderItem {
                        .geometry = geometry,
                        .batchGroup = batch->batchGroup,
                        .instanceSet = instanceSet,
                        .pso = batch->pso,
                        .args = {section.args}
                    });
                }
            }
        }
    }

    void RenderMaterialPrimitive::SetBoneData(const std::vector<RDDynamicUniformBufferPtr> &boneData) noexcept
    {
        for (uint32_t i = 0; i < sections.size(); ++i) {
            auto& section = sections[i];
            for (auto& batch : section.batches) {
                batch->boneBuffer = boneData[i].Get();
            }
        }
    }

    void RenderMaterialPrimitive::SetVertexFlags(const RenderVertexFlags& flags) noexcept
    {
        for (auto& section : sections) {
            for (auto& batch : section.batches) {
                batch->techInst.SetVertexFlags(flags);
            }
        }
    }

} // namespace sky
