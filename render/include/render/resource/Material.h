//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <unordered_map>
#include <core/type/Any.h>
#include <render/resource/Texture.h>
#include <render/resource/Technique.h>
#include <render/resource/ResourceGroup.h>

namespace sky {

    class Material {
    public:
        Material()  = default;
        ~Material() = default;

        void AddTechnique(const RDGfxTechPtr &technique);

        const RDResourceLayoutPtr &GetLayout() const { return layout; }
        const std::vector<RDGfxTechPtr> &GetGfxTechniques() const { return gfxTechniques; }

        RDResourceGroupPtr RequestResourceGroup();

    private:
        RDResourceLayoutPtr layout;
        rhi::DescriptorSetPoolPtr pool;
        std::vector<RDGfxTechPtr> gfxTechniques;
    };

    using RDMaterialPtr = std::shared_ptr<Material>;

    class MaterialInstance {
    public:
        MaterialInstance() = default;
        ~MaterialInstance() = default;

        void SetMaterial(const RDMaterialPtr &mat);

        template <class T>
        void SetValue(const std::string &key, const T &val)
        {
            SetValue(key, reinterpret_cast<const uint8_t*>(&val), sizeof(T));
        }
        void SetValue(const std::string &key, const uint8_t *t, uint32_t size);
        void SetTexture(const std::string &key, const RDTexturePtr &tex, uint32_t index = 0);
        void Upload();

        const RDResourceGroupPtr &GetResourceGroup() const { return resourceGroup; }
        const RDMaterialPtr &GetMaterial() const { return material; }

    private:
        RDMaterialPtr material;
        RDResourceGroupPtr resourceGroup;

        bool resDirty = true;

        std::unordered_map<uint32_t, RDTexturePtr> textures;
        std::unordered_map<uint32_t, RDDynamicUniformBufferPtr> uniformBuffers;
    };

    using RDMaterialInstancePtr = std::shared_ptr<MaterialInstance>;
} // namespace sky