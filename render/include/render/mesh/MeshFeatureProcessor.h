//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <render/mesh/SkeletonMeshRenderer.h>
#include <render/mesh/StaticMeshRenderer.h>

namespace sky {

    class MeshFeatureProcessor : public IFeatureProcessor {
    public:
        MeshFeatureProcessor() = default;
        ~MeshFeatureProcessor() = default;

        void Tick(float time);
        void Render(float time);

        StaticMeshRenderer *CreateStaticMesh();
        void RemoveStaticMesh(StaticMeshRenderer *mesh);

    private:
        std::list<std::unique_ptr<StaticMeshRenderer>> staticMeshes;
    };

} // namespace sky