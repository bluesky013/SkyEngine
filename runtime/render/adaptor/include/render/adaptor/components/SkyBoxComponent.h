//
// Created by blues on 2024/9/28.
//

#pragma once

#include <core/math/Color.h>
#include <framework/world/Component.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <render/adaptor/assets/TechniqueAsset.h>
#include <render/env/SkySphereRenderer.h>

namespace sky {

    struct SkyBoxData {
        Uuid skybox;
    };

    class SkyBoxComponent : public ComponentAdaptor<SkyBoxData> {
    public:
        SkyBoxComponent();
        ~SkyBoxComponent() override = default;

        COMPONENT_RUNTIME_INFO(SkyBoxComponent)

        static void Reflect(SerializationContext *context);

        void Tick(float time) override;
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void SetImage(const Uuid &image);
        const Uuid &GetImage() const;
    private:

        TechniqueAssetPtr techAsset;
        RDTexturePtr texture;
        CounterPtr<Technique> technique;
        std::unique_ptr<SkySphereRenderer> renderer;
    };

} // namespace sky




