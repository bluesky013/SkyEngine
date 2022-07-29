//
// Created by Zach Lee on 2022/7/18.
//

#pragma once

#include <render/RenderFeature.h>
#include <render/StaticMesh.h>
#include <core/util/Macros.h>

namespace sky {

    class StaticMeshFeature : public RenderFeature {
    public:
        StaticMeshFeature() = default;
        ~StaticMeshFeature() = default;

        SKY_DISABLE_COPY(StaticMeshFeature)

        StaticMesh* Create();

        void Release(StaticMesh* mesh);

        void GatherRenderItem(RenderScene& scene) override;

        void OnRender(RenderScene& scene) override;

    private:
        std::vector<std::unique_ptr<RenderMesh>> meshes;
    };

}