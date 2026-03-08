//
// Created by blues on 2024/12/8.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <render/resource/Buffer.h>
#include <core/math/Matrix4.h>
#include <list>
#include <memory>

namespace sky {

    static constexpr uint32_t MAX_DECALS = 16;

    struct DecalData {
        Matrix4 worldToDecal;
        Vector4 decalColor;
        Vector4 params; // x: blend factor, yzw: unused
    };

    struct DecalPassInfo {
        uint32_t decalCount;
        uint32_t pad[3];
        DecalData decals[MAX_DECALS];
    };

    class DecalInstance {
    public:
        DecalInstance() = default;
        ~DecalInstance() = default;

        void SetWorldMatrix(const Matrix4 &worldMatrix);
        void SetColor(const Vector4 &color);
        void SetBlendFactor(float blend);

        const DecalData &GetDecalData() const { return data; }

    private:
        DecalData data;
    };

    class DecalFeatureProcessor : public IFeatureProcessor {
    public:
        explicit DecalFeatureProcessor(RenderScene *scn);
        ~DecalFeatureProcessor() override = default;

        void Tick(float time) override;
        void Render(rdg::RenderGraph &rdg) override;

        DecalInstance *CreateDecal();
        void RemoveDecal(DecalInstance *decal);

        const RDUniformBufferPtr &GetDecalUBO() const { return decalUBO; }

    private:
        void UpdateDecalUBO();

        std::list<std::unique_ptr<DecalInstance>> decals;
        RDUniformBufferPtr decalUBO;
    };

} // namespace sky
