//
// Created by Copilot on 2026/2/16.
//

#include <render/hlod/HLODFeature.h>
#include <render/hlod/HLODFeatureProcessor.h>
#include <render/Renderer.h>

namespace sky {

    void HLODFeature::Init()
    {
        Renderer::Get()->RegisterRenderFeature<HLODFeatureProcessor>();
    }

} // namespace sky
