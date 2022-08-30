//
// Created by Zach Lee on 2022/5/7.
//

#pragma once

#include <render/resources/Buffer.h>
#include <render/resources/DescirptorGroup.h>
#include <render/resources/RenderResource.h>
#include <render/resources/Technique.h>
#include <render/resources/Texture.h>

namespace sky {

    struct MaterialAssetData {
        template <class Archive>
        void serialize(Archive &ar)
        {
        }
    };

    class Material : public RenderResource {
    public:
        Material()           = default;
        ~Material() override = default;

        void AddGfxTechnique(const RDGfxTechniquePtr &tech);

        const std::vector<RDGfxTechniquePtr> &GetGraphicTechniques() const;

        void InitRHI();

        RDDesGroupPtr GetMaterialSet() const;

        template <typename T>
        void UpdateValue(const std::string &name, const T &value)
        {
            auto table = matSet->GetProperTable();
            auto iter  = table->handleMap.find(name);
            if (iter == table->handleMap.end()) {
                return;
            }
            bufferViews[iter->second.binding]->Write(value, iter->second.offset);
        }

        void Update();

    private:
        std::vector<RDGfxTechniquePtr>                gfxTechniques;
        RDDesGroupPtr                                 matSet;
        RDBufferPtr                                   materialBuffer;
        std::unordered_map<uint32_t, RDBufferViewPtr> bufferViews;
        std::unordered_map<uint32_t, RDTexturePtr>    textures;
    };

    using RDMaterialPtr = std::shared_ptr<Material>;

    namespace impl {
        void          LoadFromPath(const std::string &path, MaterialAssetData &data);
        void          SaveToPath(const std::string &path, const MaterialAssetData &data);
        RDMaterialPtr CreateFromData(const MaterialAssetData &data);
    } // namespace impl

    template <>
    struct AssetTraits<Material> {
        using DataType = MaterialAssetData;

        static void LoadFromPath(const std::string &path, DataType &data)
        {
            impl::LoadFromPath(path, data);
        }

        static RDMaterialPtr CreateFromData(const DataType &data)
        {
            return impl::CreateFromData(data);
        }

        static void SaveToPath(const std::string &path, const DataType &data)
        {
            impl::SaveToPath(path, data);
        }
    };

    using MaterialAssetPtr = std::shared_ptr<Asset<Material>>;
} // namespace sky