//
// Created by blues on 2024/2/18.
//

#include <framework/interface/IModule.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/platform/PlatformBase.h>
#include <shader/ShaderCompiler.h>
#include <shader/ShaderCompilerDXC.h>
#include <shader/ShaderCompilerGlsl.h>
#include <shader/ShaderCacheManager.h>
#include <shader/ShaderFileSystem.h>
#include <rhi/Instance.h>
#include <cxxopts.hpp>

namespace sky {

    std::list<std::unique_ptr<ShaderCompilerBase>> g_Compilers;
    std::unordered_map<ShaderCompileTarget, std::vector<ShaderCompilerBase*>> g_CompilerMap;

    static ShaderCompilerBase *GetCompiler(const ShaderCompileOption& op)
    {
        const auto &list = g_CompilerMap[op.target];
        for (auto *compiler : list) {
            if (compiler->CheckOption(op)) {
                return compiler;
            }
        }
        return nullptr;
    }

    static ShaderCompileTarget GetApiByString(const std::string &rhi)
    {
        if (rhi == "Vulkan") {
            return ShaderCompileTarget::SPIRV;
        }
        if (rhi == "dx12") {
            return ShaderCompileTarget::DXIL;
        }
        if (rhi == "metal") {
            return ShaderCompileTarget::MSL;
        }
        return ShaderCompileTarget::NUM;
    }

    class ShaderModule : public IModule {
    public:
        ShaderModule() = default;

        ~ShaderModule() override
        {
            g_Compilers.clear();
        }

        bool Init(const StartArguments &args) override;

        void Tick(float delta) override {}

        void Shutdown() override;

        void Start() override;

        void RefreshShaders();
    };

    bool ShaderModule::Init(const StartArguments &args)
    {
        cxxopts::Options options("SkyEngine ShaderModule", "SkyEngine ShaderModule");
        options.allow_unrecognised_options();

        options.add_options()("e,engine", "Engine Directory",
            cxxopts::value<std::string>())("p,project", "Project Directory",
            cxxopts::value<std::string>())("i,intermediate", "Project Intermediate Directory",
            cxxopts::value<std::string>())("r,rhi", "RHI Type",
            cxxopts::value<std::string>());

        ShaderCompileTarget target = ShaderCompileTarget::SPIRV;

        cxxopts::ParseResult result;
        if (!args.args.empty()) {
            result = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());
            if (result.count("rhi") != 0u) {
                target = GetApiByString(result["rhi"].as<std::string>());
            }
        }

#ifdef SKY_EDITOR
        FilePath enginePath;
        if (result.count("engine") != 0u) {
            enginePath = FilePath(result["engine"].as<std::string>());
        }

        FilePath projectPath;
        if (result.count("project") != 0u) {
            projectPath = FilePath(result["project"].as<std::string>());
        }

        FilePath intermediatePath;
        if (result.count("intermediate") != 0u) {
            intermediatePath = FilePath(result["intermediate"].as<std::string>());
        } else {
            intermediatePath = projectPath / FilePath("Intermediate/shaders");
        }

        auto cachePath = projectPath / FilePath("products/shaders");
        auto *shaderCacheFs        = new NativeFileSystem(cachePath);
        auto *shaderIntermediateFs = new NativeFileSystem(intermediatePath);
        ShaderFileSystem::Get()->SetWorkFS(shaderCacheFs);
        ShaderFileSystem::Get()->SetCacheFS(shaderCacheFs);
        ShaderFileSystem::Get()->SetIntermediateFS(shaderIntermediateFs);
        ShaderFileSystem::Get()->AddSearchPath(enginePath / FilePath("assets/shaders"));
        ShaderFileSystem::Get()->AddSearchPath(projectPath / FilePath("assets/shaders"));

        RefreshShaders();
#else
        auto *cacheFs = new NativeFileSystem(Platform::Get()->GetInternalPath());

        ShaderFileSystem::Get()->SetWorkFS(AssetDataBase::Get()->GetWorkSpaceFs());
        ShaderFileSystem::Get()->SetCacheFS(cacheFs);
        ShaderFileSystem::Get()->AddSearchPath(FilePath("shaders"));
#endif

        ShaderCompiler::Get()->LoadPipelineOptions("pipeline/pass_options.hlslh");

        if (target < ShaderCompileTarget::NUM) {
            ShaderCacheManager::Get()->LoadMappingFile(target);
        }
        return true;
    }

    void ShaderModule::Shutdown()
    {
        ShaderCacheManager::Get()->SaveMappingFile();
    }

    void ShaderModule::Start()
    {
#if _WIN32
        auto *dxc = new ShaderCompilerDXC();
        if (dxc->Init()) {
            g_Compilers.emplace_back(dxc);
            g_CompilerMap[ShaderCompileTarget::SPIRV].emplace_back(dxc);
            g_CompilerMap[ShaderCompileTarget::DXIL].emplace_back(dxc);
        }
#endif
        auto *glsl = new ShaderCompilerGlsl();
        if (glsl->Init()) {
            g_Compilers.emplace_back(glsl);
            g_CompilerMap[ShaderCompileTarget::MSL].emplace_back(glsl);
            g_CompilerMap[ShaderCompileTarget::SPIRV].emplace_back(glsl);
        }
    }

    void ShaderModule::RefreshShaders()
    {
        const auto &paths = ShaderFileSystem::Get()->GetSearchPaths();
        for (const auto &path : paths) {
            auto result = NativeFileSystem::FilterFiles(path, ".hlsl");

            for (auto &file : result) {
                auto shaderName = file.GetStr();
                std::string source = ShaderCompiler::Get()->LoadShader(shaderName);
                ShaderFileSystem::Get()->SaveCacheSource(Name(shaderName.c_str()), source);
            }
        }
    }

    extern "C" SKY_EXPORT bool
    CompileBinary(const ShaderSourceDesc &desc, const ShaderCompileOption &op, ShaderBuildResult &result)
    {
        auto *compiler = GetCompiler(op);
        if (compiler == nullptr) {
            return false;
        }
        return compiler->CompileBinary(desc, op, result);;
    }

    extern "C" SKY_EXPORT ShaderCompilerBase*
    GetBinaryCompiler(const ShaderCompileOption &opt)
    {
        return GetCompiler(opt);
    }

} // namespace sky

REGISTER_MODULE(sky::ShaderModule)