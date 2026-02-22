//
// Created by blues on 2024/12/7.
//

#pragma once

#include <framework/interface/IModule.h>
#include <rhi/Instance.h>

namespace sky {

    class RenderModule : public IModule {
    public:
        RenderModule() = default;
        ~RenderModule() override = default;

        bool Init(const StartArguments &args) override;
        void Tick(float delta) override;
        void Shutdown() override;
        void Start() override;
    private:
        void ProcessArgs(const StartArguments &args);
        void InitFeatures();

        rhi::API api = rhi::API::DEFAULT;
    };

} // namespace sky
