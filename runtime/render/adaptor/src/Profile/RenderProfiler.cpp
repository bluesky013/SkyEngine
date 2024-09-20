//
// Created by blues on 2024/9/19.
//

#include <render/adaptor/profile/RenderProfiler.h>
#include <render/text/TextRegistry.h>
#include <render/text/TextFeature.h>
#include <render/RenderScene.h>
#include <framework/asset/AssetDataBase.h>

namespace sky {

    RenderProfiler::RenderProfiler(RenderScene *scn)
        : scene(scn)
    {
        auto fs = AssetDataBase::Get()->GetEngineFs();
        font = TextRegistry::Get()->LoadFont(fs, "fonts/OpenSans-Regular.ttf");
        text = scene->GetFeature<TextFeatureProcessor>()->CreateText(font);
        text->Init(TextDesc{20});

        text->AddText("test", Vector2{10, 10}, {Color{1.f, 0, 0, 1.f}});
    }


    RenderProfiler::~RenderProfiler()
    {
        if (text != nullptr) {
            scene->GetFeature<TextFeatureProcessor>()->RemoveText(text);
        }
    }

} // namespace sky