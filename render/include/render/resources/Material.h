//
// Created by Zach Lee on 2022/5/7.
//


#pragma once

#include <render/resources/RenderResource.h>
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

    private:
        std::vector<RDGfxTechniquePtr> gfxTechniques;
    };

    using RDMaterialPtr = std::shared_ptr<Material>;
}