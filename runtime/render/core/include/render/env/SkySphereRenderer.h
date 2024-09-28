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

    class SkySphereRenderer {
    public:
        SkySphereRenderer();
        ~SkySphereRenderer();

        void SetTechnique(const RDGfxTechPtr &mat);
        void SetReady();

        RenderPrimitive* GetPrimitive() const { return primitive.get(); }
        const RDResourceGroupPtr &GetResGroup() const { return resourceGroup; }
    private:
        void BuildSphere();

        RDGfxTechPtr technique;
        RDTexturePtr texture;
        RDResourceGroupPtr resourceGroup;
        rhi::DescriptorSetPoolPtr pool;
        float radius = 500.f;
        std::unique_ptr<RenderPrimitive> primitive;
    };

} // namespace sky