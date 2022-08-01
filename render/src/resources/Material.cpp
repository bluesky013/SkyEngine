//
// Created by Zach Lee on 2022/5/7.
//


#include <render/resources/Material.h>

namespace sky {

    void Material::AddGfxTechnique(RDGfxTechniquePtr tech)
    {
        gfxTechniques.emplace_back(tech);
    }

    const std::vector<RDGfxTechniquePtr>& Material::GetGraphicTechniques() const
    {
        return gfxTechniques;
    }

}