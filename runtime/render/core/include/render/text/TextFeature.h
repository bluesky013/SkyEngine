//
// Created by blues on 2024/9/13.
//

#pragma once

#include <core/environment/Singleton.h>
#include <render/resource/ResourceGroup.h>
#include <render/resource/Technique.h>
#include <render/FeatureProcessor.h>
#include <render/text/Text.h>
#include <rhi/Device.h>

namespace sky {

    class TextFeature : public Singleton<TextFeature> {
    public:
        TextFeature() = default;
        ~TextFeature() override = default;

        void Init();
        void SetTechnique(const RDGfxTechPtr &tech);
    private:
        RDGfxTechPtr technique;
        RDResourceLayoutPtr batchLayout;
        rhi::DescriptorSetPoolPtr pool;

        std::unordered_map<std::string, FontPtr> fonts;
    };

    class TextFeatureProcessor : public IFeatureProcessor {
    public:
        explicit TextFeatureProcessor(RenderScene *scn) : IFeatureProcessor(scn) {}
        ~TextFeatureProcessor() override = default;

        Text* CreateText(const FontPtr &font);
        void RemoveText(Text *text);

    private:
        void Tick(float time) override {}
        void Render(rdg::RenderGraph &rdg) override {}

        std::list<std::unique_ptr<Text>> texts;
    };

} // namespace sky
