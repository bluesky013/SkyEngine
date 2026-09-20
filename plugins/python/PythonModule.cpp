//
// Created by blues on 2024/6/2.
//

#include <framework/interface/IModule.h>
#include <python/PythonApi.h>
#include <python/PythonEngine.h>
#include <core/environment/Environment.h>

#include <string>

namespace sky::py {

    void PythonAttachEnvironment(void *environment)
    {
        Environment::Attach(static_cast<Environment *>(environment));
    }

    void PythonDetachEnvironment()
    {
        Environment::Detach();
    }

    bool PythonInit()
    {
        return PythonEngine::Get()->Init();
    }

    void PythonShutdown()
    {
        PythonEngine::Get()->Shutdown();
    }

    bool PythonRunString(const char *source)
    {
        return source != nullptr && PythonEngine::Get()->RunString(source);
    }

    bool PythonRunFile(const char *path)
    {
        return path != nullptr && PythonEngine::Get()->RunFile(path);
    }

    static const char *SCRIPT_ARG_PREFIX = "--python-script=";

    class PythonModule : public IModule {
    public:
        PythonModule() = default;
        ~PythonModule() override = default;

        bool Init(const StartArguments &args) override;
        void Tick(float delta) override;
        void Shutdown() override;

    private:
        bool ProcessArgs(const StartArguments &args);
    };

    bool PythonModule::ProcessArgs(const StartArguments &args)
    {
        if (!PythonEngine::Get()->Init()) {
            return false;
        }

        const std::string prefix = SCRIPT_ARG_PREFIX;
        for (const auto &value : args.values) {
            if (value.rfind(prefix, 0) == 0) {
                const std::string path = value.substr(prefix.size());
                if (!path.empty() && !PythonEngine::Get()->RunFile(path)) {
                    return false;
                }
            }
        }

        return true;
    }

    bool PythonModule::Init(const StartArguments &args)
    {
        if (!ProcessArgs(args)) {
            return false;
        }

        return true;
    }

    void PythonModule::Tick(float delta)
    {
    }

    void PythonModule::Shutdown()
    {
        PythonEngine::Get()->Shutdown();
        PythonEngine::Destroy();
    }

} // namespace sky::py

REGISTER_MODULE(sky::py::PythonModule)
