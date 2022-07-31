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
        StaticMeshFeature(RenderScene& scn) : RenderFeature(scn) {}
        ~StaticMeshFeature() = default;

        SKY_DISABLE_COPY(StaticMeshFeature)

        StaticMesh* Create();

        void Release(StaticMesh* mesh);

        void GatherRenderProxy() override;

        void OnRender() override;
    private:
        std::vector<std::unique_ptr<RenderMesh>> meshes;
    };

}