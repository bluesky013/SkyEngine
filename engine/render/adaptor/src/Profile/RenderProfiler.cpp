//
// Created by blues on 2024/9/19.
//

#include <render/adaptor/profile/RenderProfiler.h>
#include <render/text/TextRegistry.h>
#include <render/text/TextFeature.h>
#include <render/RenderScene.h>
#include <render/Renderer.h>
#include <render/RHI.h>
#include <framework/asset/AssetDataBase.h>
#include <sstream>

namespace sky {

    RenderProfiler::RenderProfiler(RenderScene *scn)
        : scene(scn)
        , fps(1.f)
    {

#if SKY_EDITOR
        auto fs = AssetDataBase::Get()->GetEngineFs();
#else
        auto fs = AssetDataBase::Get()->GetWorkSpaceFs();
#endif
        font = TextRegistry::Get()->LoadFont(fs, "fonts/OpenSans-Regular.ttf");
        text = scene->GetFeature<TextFeatureProcessor>()->CreateText(font);
        text->Init(TextDesc{20});
    }

    RenderProfiler::~RenderProfiler()
    {
        if (text != nullptr) {
            scene->GetFeature<TextFeatureProcessor>()->RemoveText(text);
        }
    }

    void RenderProfiler::UpdateText()
    {
        TextInfo info = {};
        info.color = Color{1.f, 1, 1, 1.f};
        info.flags = TextFlagBit::BOLD;

        std::stringstream ss;
        ss << RHI::Get()->GetDevice()->GetDeviceInfo() << " <" << RHI::Get()->GetBackendName() << "> \n";
        ss << "Render Resolution: [" << displayWidth << "," << displayHeight << "] \n";
        ss << "FPS: " << fps.GetFps() << "\n";

        const auto *pipeline = Renderer::Get()->GetPipeline();
        if (pipeline != nullptr && pipeline->Context() != nullptr) {
            const auto &data = pipeline->Context()->rdgData;
//            ss << "Triangles: " << data.triangleData << "\n";
            ss << "DrawCalls: " << data.drawCall << "\n";
        }

        text->Reset(*scene);

        const float margin = 20.f;
        const float screenW = static_cast<float>(displayWidth);
        float curY = margin;

        // render each line right-aligned
        std::istringstream iss(ss.str());
        std::string line;
        while (std::getline(iss, line)) {
            const auto lineSize = text->MeasureText(line, info);
            const float x = screenW - lineSize.x - margin;
            text->AddText(line, Vector2{x, curY}, info);
            curY += lineSize.y;
        }

        text->Finalize(*scene);
    }

    void RenderProfiler::Tick()
    {
        fps.Update();
        UpdateText();
    }

    void RenderProfiler::SetDisplaySize(uint32_t w, uint32_t h)
    {
        displayWidth = w;
        displayHeight = h;
        text->SetDisplaySize(static_cast<float>(w), static_cast<float>(h));

        UpdateText();
    }
} // namespace sky
