//
// Created by blues on 2025/2/8.
//

#include <render/RenderTechniqueLibrary.h>

namespace sky {

    void RenderTechniqueLibrary::RegisterGfxTech(const Name& name, const RDTechniquePtr &tech)
    {
        techniques.emplace(name, tech);
    }

    RDGfxTechPtr RenderTechniqueLibrary::FetchGfxTechnique(const Name &name)
    {
        auto iter = techniques.find(name);
        if (iter != techniques.end()) {
            RDGfxTechPtr tech = static_cast<GraphicsTechnique*>(iter->second.Get());
            return tech;
        }
        return {};
    }

} // namespace sky