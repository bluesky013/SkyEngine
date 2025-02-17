//
// Created by blues on 2023/12/18.
//

#pragma once

#include <render/resource/Buffer.h>
#include <render/resource/Mesh.h>
#include <render/mesh/StandardMeshDefines.h>
#include <render/RenderPrimitive.h>

namespace sky {
    class RenderScene;

    struct SkyBoxVertex {
        Vector4 pos;
        Vector2 uv;
    };

    struct SkySpherePrimitive : public RenderPrimitive {
        void PrepareBatch() override {}
        void UpdateBatch() override;
        bool IsReady() const;

        RDTexture2DPtr texture;
        rhi::DescriptorSetPoolPtr pool;
    };

    class SkySphereRenderer {
    public:
        SkySphereRenderer();
        ~SkySphereRenderer();

        void SetTechnique(const RDGfxTechPtr &mat);

        SkySpherePrimitive* GetPrimitive() const { return primitive.get(); }
    private:
        void BuildSphere();

        RDGfxTechPtr technique;
        RDTexturePtr texture;
        rhi::DescriptorSetPoolPtr pool;
        float radius = 500.f;
        std::unique_ptr<SkySpherePrimitive> primitive;
    };

} // namespace sky