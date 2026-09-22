//
// Created on 2026/09/21.
//
// Thin editor module: delegates to the editor-owned renderer (EditorRender).
//

#include <editor/sandbox/SandboxModule.h>

#include <core/cmdline/CmdParser.h>
#include <core/logger/Logger.h>

static const char *TAG = "SandboxModule";

namespace sky::editor {

    namespace {
        sky::aurora::API ParseApi(const std::string &name)
        {
            if (name == "vulkan" || name == "vk") {
                return sky::aurora::API::VULKAN;
            }
            if (name == "dx12" || name == "d3d12") {
                return sky::aurora::API::DX12;
            }
            if (name == "metal") {
                return sky::aurora::API::METAL;
            }
            return sky::aurora::API::DEFAULT;
        }

        sky::aurora::API ParseApiArgs(const sky::StartArguments &args)
        {
            if (args.args.empty()) {
                return sky::aurora::API::DEFAULT;
            }
            sky::CmdOptions options("SandboxEditor", "SkyEngine Editor");
            options.allow_unrecognised_options();
            options.add_options()("r,rhi", "RHI Type", sky::CmdValue<std::string>());

            auto result = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());
            if (result.count("rhi") != 0u) {
                return ParseApi(result["rhi"].as<std::string>());
            }
            return sky::aurora::API::DEFAULT;
        }
    } // namespace

    SandboxModule::SandboxModule() = default;
    SandboxModule::~SandboxModule() = default;

    bool SandboxModule::Init(const sky::StartArguments &args)
    {
        const auto api = ParseApiArgs(args);
        initialized = renderer.Init("SandboxEditor", 1280, 720, api);
        return initialized;
    }

    void SandboxModule::Start()
    {
        if (initialized) {
            renderer.Start();
        }
    }

    void SandboxModule::Tick(float delta)
    {
        if (initialized) {
            renderer.Tick(delta);
        }
    }

    void SandboxModule::Shutdown()
    {
        if (initialized) {
            renderer.Shutdown();
        }
        initialized = false;
    }

} // namespace sky::editor
