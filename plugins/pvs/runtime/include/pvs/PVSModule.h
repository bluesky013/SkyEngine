//
// Created by Zach Lee on 2026/2/23.
//

#pragma once

#include <framework/interface/IModule.h>
#include <render/Renderer.h>

namespace sky {
    class PVSCulling;

    class PVSModule
        : public IModule
        , public IRenderSystemEvent {
    public:
        PVSModule() = default;
        ~PVSModule() override = default;

        bool Init(const StartArguments &args) override;

        void Start() override;

        void Shutdown() override;

        void OnCreateRenderScene(RenderScene* scene) override;

    protected:
        EventBinder<IRenderSystemEvent> binder;
        PVSCulling *pvsCulling = nullptr; // weakRef
    };

} // namespace sky