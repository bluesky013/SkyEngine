//
// Created by Zach Lee on 2022/5/7.
//

#include <cereal/archives/json.hpp>
#include <framework/asset/AssetManager.h>
#include <render/Render.h>
#include <render/resources/Material.h>

namespace sky {

    void Material::AddGfxTechnique(const RDGfxTechniquePtr &tech)
    {
        gfxTechniques.emplace_back(tech);
    }

    const std::vector<RDGfxTechniquePtr> &Material::GetGraphicTechniques() const
    {
        return gfxTechniques;
    }

    void Material::InitRHI()
    {
        if (gfxTechniques.empty()) {
            return;
        }
        auto &tech            = gfxTechniques[0];
        auto  shaderTable     = tech->GetShaderTable();
        matSet                = shaderTable->CreateDescriptorGroup(2);
        auto &descriptorTable = matSet->GetRHISet()->GetLayout()->GetDescriptorTable();

        Buffer::Descriptor descriptor = {};
        descriptor.usage              = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        descriptor.memory             = VMA_MEMORY_USAGE_GPU_ONLY;
        descriptor.allocCPU           = true;
        descriptor.size               = 0;

        materialBuffer      = std::make_shared<Buffer>();
        auto defaultTexture = Render::Get()->GetDefaultTexture();
        for (auto &[binding, info] : descriptorTable) {
            if (info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
                bufferViews[binding] = std::make_shared<BufferView>(materialBuffer, info.size, descriptor.size);
                descriptor.size += info.size;
            } else if (info.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                textures[binding] = defaultTexture;
            }
        }
        materialBuffer->Init(descriptor);
        materialBuffer->InitRHI();

        for (auto &[binding, view] : bufferViews) {
            matSet->UpdateBuffer(binding, view);
        }

        for (auto &[binding, tex] : textures) {
            matSet->UpdateTexture(binding, tex);
        }
        matSet->Update();
    }

    RDDesGroupPtr Material::GetMaterialSet() const
    {
        return matSet;
    }

    void Material::Update()
    {
        for (auto &view : bufferViews) {
            view.second->RequestUpdate();
        }
        matSet->Update();
    }

    std::shared_ptr<Material> Material::CreateFromData(const MaterialAssetData &data)
    {
        auto mat = std::make_shared<Material>();
        for (auto &prop : data.properties) {
            if (prop.type == MaterialPropertyType::FLOAT) {
                mat->UpdateValue(prop.name, *prop.any.GetAsConst<float>());
            } else if (prop.type == MaterialPropertyType::INT) {
                mat->UpdateValue(prop.name, *prop.any.GetAsConst<int32_t>());
            } else if (prop.type == MaterialPropertyType::UINT) {
                mat->UpdateValue(prop.name, *prop.any.GetAsConst<uint32_t>());
            } else if (prop.type == MaterialPropertyType::INT64) {
                mat->UpdateValue(prop.name, *prop.any.GetAsConst<int64_t>());
            } else if (prop.type == MaterialPropertyType::UINT64) {
                mat->UpdateValue(prop.name, *prop.any.GetAsConst<uint64_t>());
            } else if (prop.type == MaterialPropertyType::DOUBLE) {
                mat->UpdateValue(prop.name, *prop.any.GetAsConst<double>());
            } else if (prop.type == MaterialPropertyType::VEC2) {
                mat->UpdateValue(prop.name, *prop.any.GetAsConst<Vector2>());
            } else if (prop.type == MaterialPropertyType::VEC3) {
                mat->UpdateValue(prop.name, *prop.any.GetAsConst<Vector3>());
            } else if (prop.type == MaterialPropertyType::VEC4) {
                mat->UpdateValue(prop.name, *prop.any.GetAsConst<Vector4>());
            } else if (prop.type == MaterialPropertyType::BOOL) {
                mat->UpdateValue(prop.name, *prop.any.GetAsConst<bool>());
            } else if (prop.type == MaterialPropertyType::TEXTURE) {
                auto imageAssetId = *prop.any.GetAsConst<Uuid>();
                auto imageAsset = AssetManager::Get()->LoadAsset<Image>(imageAssetId);
                if (imageAsset) {
                    auto image = imageAsset->CreateInstance();
                    mat->UpdateTexture(prop.name, Texture::CreateFromImage(image, {}));
                } else {
                    mat->UpdateTexture(prop.name, Render::Get()->GetDefaultTexture());
                }
            }
        }
        mat->InitRHI();
        return mat;
    }
} // namespace sky