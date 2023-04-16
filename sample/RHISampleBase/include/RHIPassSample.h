//
// Created by Zach Lee on 2023/4/7.
//

#pragma once

#include <RHISampleBase.h>
#include <scene_render/Material.h>
#include <scene_render/Scene.h>

namespace sky::rhi {

    class RHIPassSample : public RHISampleBase {
    public:
        RHIPassSample() = default;
        ~RHIPassSample() = default;

        void SetupBase() override;
        void OnTick(float delta) override;
        void OnStop() override;
    private:
        void SetupMaterial();
        void SetupMesh();
        void SetupCamera();
        void SetupLayout();
        void SetupScene();
        void SetupPipeline();

        MaterialPtr material;
        CameraPtr camera;
        MeshPtr mesh;
        std::shared_ptr<Scene> scene;

        GFXTechniquePtr gfxTech;
        DescriptorSetLayoutPtr globalLayout;
        DescriptorSetLayoutPtr localLayout;
        BufferPtr cameraBuffer;
    };

}