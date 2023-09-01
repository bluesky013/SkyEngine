//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <vector>
#include <render/resource/Shader.h>

namespace sky {

    class Technique {
    public:
        Technique() = default;
        virtual ~Technique() = default;
    };

    class GraphicsTechnique : public Technique {
    public:
        GraphicsTechnique() = default;
        ~GraphicsTechnique() override = default;

    private:
        rhi::GraphicsPipeline::Descriptor desc;
    };

    class ComputeTechnique : public Technique {
    public:
        ComputeTechnique() = default;
        ~ComputeTechnique() override = default;
    };

} // namespace sky
