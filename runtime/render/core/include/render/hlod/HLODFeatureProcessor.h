//
// Created by Copilot on 2026/2/16.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <render/hlod/HLODRenderer.h>
#include <list>
#include <memory>

namespace sky {

    class HLODFeatureProcessor : public IFeatureProcessor {
    public:
        explicit HLODFeatureProcessor(RenderScene *scene) : IFeatureProcessor(scene) {}
        ~HLODFeatureProcessor() override = default;

        void Tick(float time) override;
        void Render(rdg::RenderGraph &rdg) override;

        HLODRenderer *CreateHLODRenderer();
        void RemoveHLODRenderer(HLODRenderer *renderer);

    private:
        std::list<std::unique_ptr<HLODRenderer>> renderers;
    };

} // namespace sky
