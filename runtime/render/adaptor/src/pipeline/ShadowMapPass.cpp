//
// Created by blues on 2024/9/6.
//

#include <render/adaptor/pipeline/ShadowMapPass.h>
#include <core/math/Transform.h>

namespace sky {

    ShadowMapPass::ShadowMapPass(uint32_t width_, uint32_t height_)
        : RasterPass(Name("ShadowMap"))
    {
        width  = width_;
        height = height_;

        // TODO
        Transform trans = {};
        trans.translation = Vector3(21, 28, -7);
        trans.rotation.FromEulerYZX(Vector3(-50, 113, 0));

//        shadowScene = scenes[0]->CreateSceneView(1);
//        shadowScene->SetPerspective(1.f, 100.f, 75.f / 180.f * PI, 1.f);
//        shadowScene->SetMatrix(trans.ToMatrix());
//        shadowScene->Update();

        rdg::GraphImage image = {};
        image.extent.width  = width;
        image.extent.height = height;
        image.usage         = rhi::ImageUsageFlagBit::SAMPLED | rhi::ImageUsageFlagBit::DEPTH_STENCIL;
        image.format        = rhi::PixelFormat::D32;

        images.emplace_back(Name(SHADOW_MAP.data()), image);
    }

    void ShadowMapPass::SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene)
    {
    }
} // namespace sky