//
// Created by blues on 2023/12/18.
//

#include <render/env/SkyBoxRenderer.h>
#include <render/RenderScene.h>

namespace sky {

    SkyBoxRenderer::~SkyBoxRenderer()
    {
        if (scene != nullptr) {
            scene->RemovePrimitive(primitive.get());
        }
    }

    void SkyBoxRenderer::SetUp(const RDMaterialInstancePtr &mat)
    {
        material = mat;
        primitive = std::make_unique<RenderPrimitive>();

        const auto &techniques = mat->GetMaterial()->GetGfxTechniques();
        primitive->techniques.reserve(techniques.size());
        for (const auto &tech : techniques) {
            primitive->techniques.emplace_back(TechniqueInstance{"", tech});
        }
        primitive->batchSet = mat->GetResourceGroup();
        primitive->args.emplace_back(rhi::CmdDrawLinear {
            36,
            1,
            0,
            0
        });
    }

    void SkyBoxRenderer::AttachScene(RenderScene *scene_)
    {
        scene = scene_;
        scene->AddPrimitive(primitive.get());
    }

} // namespace sky