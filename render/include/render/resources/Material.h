//
// Created by Zach Lee on 2022/5/7.
//


#pragma once

#include <render/resources/RenderResource.h>
#include <render/resources/DescirptorGroup.h>
#include <render/resources/Technique.h>
#include <render/resources/Buffer.h>
#include <render/resources/Texture.h>

namespace sky {

    class Material : public RenderResource {
    public:
        Material() = default;
        ~Material() = default;

        void AddGfxTechnique(RDGfxTechniquePtr tech);

        const std::vector<RDGfxTechniquePtr>& GetGraphicTechniques() const;

        void InitRHI();

        RDDesGroupPtr GetMaterialSet() const;

        template <typename T>
        void UpdateValue(std::string name, const T& value)
        {
            auto table = matSet->GetProperTable();
            auto iter = table->handleMap.find(name);
            if (iter == table->handleMap.end()) {
                return;
            }
            bufferView[iter->second.binding]->Write(value, iter->second.offset);
        }

        void Update();
    private:
        std::vector<RDGfxTechniquePtr> gfxTechniques;
        RDDesGroupPtr matSet;
        RDBufferPtr materialBuffer;
        std::unordered_map<uint32_t, RDBufferViewPtr> bufferView;
    };

    using RDMaterialPtr = std::shared_ptr<Material>;
}