//
// Created by Zach Lee on 2022/5/7.
//


#include <engine/render/resources/Material.h>

namespace sky {

    void Material::AddTechnique(RDTechniquePtr tech)
    {
        techniques.emplace_back(tech);
    }

}