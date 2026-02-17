//
// Created by Zach Lee on 2023/8/19.
//

#include <render/RenderPrimitive.h>
#include <render/resource/Buffer.h>
#include <render/Renderer.h>
#include <render/RHI.h>

namespace sky {
    void RenderMaterialPrimitive::PrepareBatch()
    {
        for (auto &batch : batches) {
            batch.technique->ForEachOption([this, &batch](const Name& name) {
                const auto *val = material->GetValue<uint32_t>(name);
                if (val != nullptr) {
                    batch.SetOption(name, static_cast<uint8_t>(*val));
                }
            });
        }

        if (material) {
            material->UploadTextures();
        }
    }

    void RenderMaterialPrimitive::UpdateBatch()
    {
        batchBuffers.resize(batches.size());

        for (uint32_t i = 0; i < batches.size(); ++i) {
            auto &batch = batches[i];

            if (!batch.program) {
                continue;
            }

            auto rhiLayout = batch.program->GetPipelineLayout()->GetSetLayout(BATCH_SET);
            if (!rhiLayout) {
                batch.batchGroup = nullptr;
                continue;
            }

            bool needUpdateBuffer = batch.valueVersion != material->GetValueVersion();
            bool needUpdateBatch  = batch.batchVersion != material->GetBatchVersion();

            RDResourceLayoutPtr layout;
            if (!batch.batchGroup || rhiLayout->GetHash() != batch.batchLayoutHash) {
                layout = batch.program->RequestLayout(BATCH_SET);

                batch.batchGroup = new ResourceGroup();
                batch.batchGroup->Init(layout, *Renderer::Get()->GetMaterialManager()->GetPool());
                batch.batchLayoutHash = layout->GetRHILayout()->GetHash();

                needUpdateBatch = true;
            } else {
                layout = batch.batchGroup->GetLayout();
            }

            const auto &bindingHandlers = layout->GetBindingHandlers();
            const auto &bufferHandles = layout->GetBufferNameHandles();

            const Name bufferName("shading");  // only shader buffer is used.
            auto iter = bindingHandlers.find(bufferName);

            if (needUpdateBuffer && iter != bindingHandlers.end()) {
                auto &buffer = batchBuffers[i];
                if (!buffer) {
                    buffer = new DynamicUniformBuffer();
                    buffer->Init(iter->second.size);
                }

                auto *ptr = buffer->GetAddress();
                for (const auto &[name, handle] : bufferHandles) {
                    uint8_t *dst = ptr + handle.offset;
                    material->GetValueRaw(name, dst, handle.size);
                }

                buffer->Upload();
            }

            auto *device = RHI::Get()->GetDevice();

            if (needUpdateBatch) {
                const auto &rhiBindings = layout->GetRHILayout()->GetBindings();
                for (const auto &[name, handler] : bindingHandlers) {
                    if (name == bufferName) {
                        batch.batchGroup->BindDynamicUBO(bufferName, batchBuffers[i], 0);
                    } else {
                        auto bIter = std::find_if(rhiBindings.begin(), rhiBindings.end(), [binding = handler.binding](const auto &val) {
                            return binding == val.binding;
                        });

                        if (bIter != rhiBindings.end()) {
                            if (bIter->type == rhi::DescriptorType::COMBINED_IMAGE_SAMPLER || bIter->type == rhi::DescriptorType::SAMPLED_IMAGE) {
                                auto tex = material->GetTexture(name);
                                if (tex) {
                                    batch.batchGroup->BindTexture(name, tex->GetImageView(), 0);
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

                                    batch.batchGroup->BindSampler(name, device->CreateSampler(desc), 0);
                                }
                            }
                        }
                    }
                }

                batch.batchGroup->Update();
            }

            batch.valueVersion = material->GetValueVersion();
            batch.batchVersion = material->GetBatchVersion();
        }
    }

    bool RenderMaterialPrimitive::IsReady() const
    {
        return material && material->IsReady();
    }

} // namespace sky
