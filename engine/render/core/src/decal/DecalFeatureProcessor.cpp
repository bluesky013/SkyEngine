//
// Created by blues on 2024/12/8.
//

#include <render/decal/DecalFeatureProcessor.h>
#include <render/RHI.h>

namespace sky {

    void DecalInstance::SetWorldMatrix(const Matrix4 &worldMatrix)
    {
        data.worldToDecal = worldMatrix.Inverse();
    }

    void DecalInstance::SetColor(const Vector4 &color)
    {
        data.decalColor = color;
    }

    void DecalInstance::SetBlendFactor(float blend)
    {
        data.params.x = blend;
    }

    DecalFeatureProcessor::DecalFeatureProcessor(RenderScene *scn) : IFeatureProcessor(scn)
    {
        decalUBO = new UniformBuffer();
        decalUBO->Init(sizeof(DecalPassInfo));
    }

    DecalInstance *DecalFeatureProcessor::CreateDecal()
    {
        auto *decal = decals.emplace_back(std::make_unique<DecalInstance>()).get();
        return decal;
    }

    void DecalFeatureProcessor::RemoveDecal(DecalInstance *decal)
    {
        decals.remove_if([decal](const std::unique_ptr<DecalInstance> &d) {
            return d.get() == decal;
        });
    }

    void DecalFeatureProcessor::UpdateDecalUBO()
    {
        DecalPassInfo info = {};
        info.decalCount = 0;

        for (const auto &decal : decals) {
            if (info.decalCount >= MAX_DECALS) {
                break;
            }
            info.decals[info.decalCount++] = decal->GetDecalData();
        }

        decalUBO->WriteT(0, info);
    }

    void DecalFeatureProcessor::Tick(float /*time*/)
    {
        UpdateDecalUBO();
    }

    void DecalFeatureProcessor::Render(rdg::RenderGraph &rdg)
    {
        auto &rg = rdg.resourceGraph;
        rg.ImportUBO(Name("DECAL_PASS_INFO"), decalUBO);
    }

} // namespace sky
