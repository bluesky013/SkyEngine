//
// Created by blues on 2024/9/19.
//

#include <render/adaptor/profile/RenderProfiler.h>
#include <render/text/TextRegistry.h>
#include <render/text/TextFeature.h>
#include <render/RenderScene.h>
#include <render/RHI.h>
#include <framework/asset/AssetDataBase.h>

namespace sky {

    RenderProfiler::RenderProfiler(RenderScene *scn)
        : scene(scn)
    {
        auto fs = AssetDataBase::Get()->GetEngineFs();
        font = TextRegistry::Get()->LoadFont(fs, "fonts/OpenSans-Regular.ttf");
        text = scene->GetFeature<TextFeatureProcessor>()->CreateText(font);
        text->Init(TextDesc{20});

        TextInfo info = {};
        info.color = Color{1.f, 1, 1, 1.f};

        std::stringstream ss;
        ss << RHI::Get()->GetDevice()->GetDeviceInfo() << " <" << RHI::Get()->GetBackendName() << ">" << std::endl;
        text->AddText(ss.str(), Vector2{20, 40}, info);
        text->Finalize(*scene);
    }

    RenderProfiler::~RenderProfiler()
    {
        if (text != nullptr) {
            scene->GetFeature<TextFeatureProcessor>()->RemoveText(text);
        }
    }

    void RenderProfiler::SetDisplaySize(uint32_t w, uint32_t h)
    {
        text->SetDisplaySize(static_cast<float>(w), static_cast<float>(h));
    }
} // namespace sky