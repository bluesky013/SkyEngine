//
// Created by Zach Lee on 2023/9/3.
//

#include <render/geometry/GeometryFeature.h>

namespace sky {

    void GeometryFeature::Init(const RDGfxTechPtr &tech)
    {
        technique = tech;
    }

} // namespace sky