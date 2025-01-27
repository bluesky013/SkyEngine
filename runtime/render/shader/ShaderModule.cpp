//
// Created by blues on 2024/2/18.
//

#include <framework/interface/IModule.h>
#include <shader/ShaderCompiler.h>
#include <shader/ShaderCompilerDXC.h>
#include <shader/ShaderCompilerGlsl.h>
#include <shader/ShaderCacheManager.h>
#include <shader/ShaderFileSystem.h>
#include <rhi/Instance.h>
#include <cxxopts.hpp>

namespace sky {

    std::list<std::unique_ptr<ShaderCompilerBase>> g_Compilers;
    std::unordered_map<ShaderCompileTarget, ShaderCompilerBase*> g_CompilerMap;

    static ShaderCompilerBase *GetCompiler(ShaderCompileTarget target)
    {
        auto iter = g_CompilerMap.find(target);
        return iter != g_CompilerMap.end() ? iter->second : nullptr;
    }

    static ShaderCompileTarget GetApiByString(const std::string &rhi)
    {
        if (rhi == "vulkan") {
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

        options.add_options()
            ("e,engine", "Engine Directory", cxxopts::value<std::string>())
            ("p,project", "Project Directory", cxxopts::value<std::string>())
            ("i,intermediate", "Project Intermediate Directory", cxxopts::value<std::string>())
            ("r,rhi", "RHI Type", cxxopts::value<std::string>());

        if (!args.args.empty()) {
            auto result = options.parse(static_cast<int32_t>(args.args.size()), args.args.data());


            if (result.count("rhi") != 0u) {
                auto target = GetApiByString(result["rhi"].as<std::string>());

                if (target < ShaderCompileTarget::NUM) {
                    ShaderCacheManager::Get()->LoadMappingFile(target);
                }
            }

#ifdef SKY_EDITOR
            FilePath enginePath;
            if (result.count("engine")) {
                enginePath = FilePath(result["engine"].as<std::string>());
            }

            FilePath projectPath;
            if (result.count("project")) {
                projectPath = FilePath(result["project"].as<std::string>());
            }

            FilePath intermediatePath;
            if (result.count("intermediate") != 0u) {
                intermediatePath = FilePath(result["intermediate"].as<std::string>());
            } else {
                intermediatePath = projectPath / FilePath("Intermediate/shaders");
            }

            auto shaderCacheFs = new NativeFileSystem(projectPath / FilePath("products/shaders"));
            auto shaderIntermediateFs = new NativeFileSystem(intermediatePath);
            ShaderFileSystem::Get()->SetWorkFS(shaderCacheFs);
            ShaderFileSystem::Get()->SetIntermediateFS(shaderIntermediateFs);
            ShaderFileSystem::Get()->AddSearchPath(enginePath / FilePath("assets/shaders"));
            ShaderFileSystem::Get()->AddSearchPath(projectPath / FilePath("assets/shaders"));

            RefreshShaders();
#else

#endif
            ShaderCompiler::Get()->LoadPipelineOptions("shaders/pipeline/pass_options.hlslh");
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
            g_CompilerMap.emplace(ShaderCompileTarget::DXIL, dxc);
//            g_CompilerMap.emplace(ShaderCompileTarget::SPIRV, dxc);
        }
#endif
        auto *glsl = new ShaderCompilerGlsl();
        if (glsl->Init()) {
            g_Compilers.emplace_back(glsl);
            g_CompilerMap.emplace(ShaderCompileTarget::MSL, glsl);
            g_CompilerMap.emplace(ShaderCompileTarget::SPIRV, glsl);
        }


    }

    void ShaderModule::RefreshShaders()
    {
        const auto &paths = ShaderFileSystem::Get()->GetSearchPaths();
        for (auto &path : paths) {
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
        auto *compiler = GetCompiler(op.target);
        if (compiler == nullptr) {
            return false;
        }
        return compiler->CompileBinary(desc, op, result);;
    }

    extern "C" SKY_EXPORT ShaderCompilerBase*
    GetBinaryCompiler(ShaderCompileTarget target)
    {
        return GetCompiler(target);
    }

} // namespace sky

REGISTER_MODULE(sky::ShaderModule)