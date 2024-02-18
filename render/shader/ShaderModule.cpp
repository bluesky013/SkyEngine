//
// Created by blues on 2024/2/18.
//

#include <framework/interface/IModule.h>
#include <shader/ShaderCompiler.h>
#include <shader/ShaderCompilerDXC.h>
#include <shader/ShaderCompilerGlsl.h>

namespace sky {

    std::list<std::unique_ptr<ShaderCompilerBase>> g_Compilers;
    std::unordered_map<ShaderCompileTarget, ShaderCompilerBase*> g_CompilerMap;

    static ShaderCompilerBase *GetCompiler(ShaderCompileTarget target)
    {
        auto iter = g_CompilerMap.find(target);
        return iter != g_CompilerMap.end() ? iter->second : nullptr;
    }

    class ShaderModule : public IModule, public Singleton<ShaderModule> {
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
    private:
        void ProcessArgs(const StartArguments &args);
    };

    bool ShaderModule::Init(const StartArguments &args)
    {
        return true;
    }

    void ShaderModule::Shutdown()
    {
    }

    void ShaderModule::Start()
    {
        auto *dxc = new ShaderCompilerDXC();
        if (dxc->Init()) {
            g_Compilers.emplace_back(dxc);
            g_CompilerMap.emplace(ShaderCompileTarget::DXIL, dxc);
        }

        auto *glsl = new ShaderCompilerGlsl();
        if (glsl->Init()) {
            g_Compilers.emplace_back(glsl);
            g_CompilerMap.emplace(ShaderCompileTarget::MSL, glsl);
            g_CompilerMap.emplace(ShaderCompileTarget::SPIRV, dxc);
        }
    }

    void ShaderModule::ProcessArgs(const StartArguments &args)
    {
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
} // namespace sky

REGISTER_MODULE(sky::ShaderModule)