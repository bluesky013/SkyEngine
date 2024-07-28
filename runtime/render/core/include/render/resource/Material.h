//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <unordered_map>
#include <render/RenderResource.h>
#include <render/resource/Texture.h>
#include <render/resource/Technique.h>
#include <render/resource/ResourceGroup.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>

namespace sky {

    struct MaterialTexture {
        uint32_t texIndex;
    };

    enum class MaterialValueType : uint32_t {
        TEXTURE,
        VEC2,
        VEC3,
        VEC4,
        FLOAT,
        U32,
        I32
    };

    using MaterialValue = std::variant<MaterialTexture, Vector2, Vector3, Vector4, float, uint32_t, int32_t>;

    class Material : public RenderResource {
    public:
        Material()  = default;
        ~Material() override = default;

        void AddTechnique(const RDGfxTechPtr &technique);

        const RDResourceLayoutPtr &GetLayout() const { return layout; }
        const std::vector<RDGfxTechPtr> &GetGfxTechniques() const { return gfxTechniques; }

        RDResourceGroupPtr RequestResourceGroup();

    private:
        RDResourceLayoutPtr layout;
        rhi::DescriptorSetPoolPtr pool;
        std::vector<RDGfxTechPtr> gfxTechniques;
    };

    using RDMaterialPtr = CounterPtr<Material>;

    class MaterialInstance : public RenderResource {
    public:
        MaterialInstance() = default;
        ~MaterialInstance() override = default;

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

    using RDMaterialInstancePtr = CounterPtr<MaterialInstance>;
} // namespace sky