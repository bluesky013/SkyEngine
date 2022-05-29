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

        void AddTechnique(RDTechniquePtr tech);

    private:
        std::vector<RDTechniquePtr> techniques;
    };

    using RDMaterialPtr = std::shared_ptr<Material>;
}