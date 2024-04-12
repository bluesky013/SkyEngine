//
// Created by Zach Lee on 2023/8/27.
//

#pragma once

#include <framework/interface/IRenderScene.h>
#include <render/RenderScene.h>

namespace sky {

    class RenderSceneProxy : public IRenderScene {
    public:
        RenderSceneProxy();
        ~RenderSceneProxy() override;

        RenderScene *GetRenderScene() const { return renderScene; }

    private:
        RenderScene *renderScene = nullptr;
    };

} // namespace sky