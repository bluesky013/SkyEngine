//
// Created by Copilot on 2026/2/16.
//

#include <render/hlod/HLODFeatureProcessor.h>

namespace sky {

    void HLODFeatureProcessor::Tick(float time)
    {
        for (auto &renderer : renderers) {
            renderer->Tick();
        }
    }

    void HLODFeatureProcessor::Render(rdg::RenderGraph &rdg)
    {
    }

    HLODRenderer *HLODFeatureProcessor::CreateHLODRenderer()
    {
        auto *renderer = new HLODRenderer();
        renderer->AttachScene(scene);
        return renderers.emplace_back(renderer).get();
    }

    void HLODFeatureProcessor::RemoveHLODRenderer(HLODRenderer *renderer)
    {
        renderers.remove_if([renderer](const auto &val) {
            return renderer == val.get();
        });
    }

} // namespace sky
