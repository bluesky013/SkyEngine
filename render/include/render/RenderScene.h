//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <vector>
#include <core/std/Container.h>
#include <render/SceneView.h>

namespace sky {

    class RenderScene {
    public:
        RenderScene() = default;
        ~RenderScene() = default;

    private:
        PmrUnSyncPoolRes resources;
        PmrVector<ViewPtr> sceneViews;
    };

} // namespace sky