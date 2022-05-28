//
// Created by Zach Lee on 2022/5/7.
//


#pragma once

#include <engine/render/resources/RenderResource.h>
#include <engine/render/resources/Technique.h>
#include <engine/render/resources/Buffer.h>
#include <engine/render/resources/Texture.h>

namespace sky {

    class Material : public RenderResource {
    public:
        Material() = default;
        ~Material() = default;

        void AddTechnique(RDTechniquePtr tech);

    private:
        std::vector<RDTechniquePtr> techniques;
    };

    using RDMaterialPtr = std::shared_ptr<Material>;
}