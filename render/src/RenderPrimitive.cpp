//
// Created by Zach Lee on 2022/8/1.
//

#include <render/RenderPrimtive.h>

namespace sky {

    void RenderPrimitive::SetMaterial(RDMaterialPtr value)
    {
        material = value;

        {
            auto& techniques = material->GetGraphicTechniques();
            for (auto& tech : techniques) {
                auto proxy = new GraphicsTechniqueProxy();
                proxy->gfxTechnique = tech;
                proxy->setBinder = std::make_unique<drv::DescriptorSetBinder>();
                proxy->assembly = vertexAssembly;
                proxy->drawTag |= tech->GetDrawTag();
                proxy->args = &drawArgs;

                SetViewTag(tech->GetViewTag());
                graphicTechniques.emplace_back(proxy);
            }
        }

    }

}