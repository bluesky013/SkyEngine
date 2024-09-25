//
// Created by blues on 2024/2/28.
//

#include <framework/interface/IModule.h>
#include <framework/platform/PlatformBase.h>

#include <memory>

#include <xr/XRInstance.h>
#include <render/RHI.h>

#include <cxxopts.hpp>

namespace sky {

    class XRModule : public IModule {
    public:
        XRModule() = default;
        ~XRModule() override = default;

        bool Init(const StartArguments &args) override;
        void Tick(float delta) override;
        void PreTick(float delta) override;
        void PostTick(float delta) override;

    private:
        bool ProcessArgs(const StartArguments &args);

        std::unique_ptr<XRInstance> instance;
        rhi::API api = rhi::API::DEFAULT;
    };

    bool XRModule::ProcessArgs(const StartArguments &args)
    {
        cxxopts::Options options("SkyEngine XR Plugin", "SkyEngine XR Plugin");
        options.allow_unrecognised_options();

        options.add_options()("r,rhi", "RHI Type", cxxopts::value<std::string>());
        options.add_options()("a,app", "App Type", cxxopts::value<std::string>());

        if (args.args.empty()) {
            return false;
        }

        auto result = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());

        if (result.count("app") == 0u || result["app"].as<std::string>() != "xr") {
            return false;
        }

        if (result.count("rhi") != 0u) {
            api = rhi::GetApiByString(result["rhi"].as<std::string>());
        }

        return true;
    }

    bool XRModule::Init(const StartArguments &args)
    {
        if (!ProcessArgs(args)) {
            return false;
        }

        instance = std::make_unique<XRInstance>();
        return instance->Init({api});
    }

    void XRModule::PreTick(float delta)
    {
        if (instance) {
            instance->BeginFrame();
        }
    }

    void XRModule::PostTick(float delta)
    {
        if (instance) {
            instance->EndFrame();
        }
    }

    void XRModule::Tick(float delta)
    {
    }
}
REGISTER_MODULE(sky::XRModule)
