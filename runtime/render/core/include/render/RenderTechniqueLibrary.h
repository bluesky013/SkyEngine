//
// Created by blues on 2025/2/8.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/name/Name.h>
#include <render/resource/Technique.h>
#include <unordered_map>

namespace sky {

    class RenderTechniqueLibrary : public Singleton<RenderTechniqueLibrary> {
    public:
        RenderTechniqueLibrary() = default;
        ~RenderTechniqueLibrary() override = default;

        void RegisterGfxTech(const Name& name, const RDTechniquePtr &tech);
        RDGfxTechPtr FetchGfxTechnique(const Name &name);

    private:
        std::unordered_map<Name, RDGfxTechPtr> techniques;
    };

} // namespace sky
