//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <render/mesh/MeshRenderer.h>
#include <render/skeleton/SkeletalMeshRenderer.h>

namespace sky {

    class MeshFeatureProcessor : public IFeatureProcessor {
    public:
        explicit MeshFeatureProcessor(RenderScene *scene) : IFeatureProcessor(scene) {}
        ~MeshFeatureProcessor() override = default;

        void Tick(float time) override;
        void Render(rdg::RenderGraph &rdg) override;

        MeshRenderer *CreateStaticMesh();
        void RemoveStaticMesh(MeshRenderer *mesh);

        SkeletalMeshRenderer *CreateSkeletalMesh();
        void RemoveSkeletalMesh(SkeletalMeshRenderer *mesh);

    private:
        std::list<std::unique_ptr<MeshRenderer>> staticMeshes;

        std::list<std::unique_ptr<SkeletalMeshRenderer>> skeletonMeshes;
    };

} // namespace sky
