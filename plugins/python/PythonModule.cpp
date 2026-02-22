//
// Created by blues on 2024/6/2.
//

#include <framework/interface/IModule.h>
#include <python/PythonEngine.h>

namespace sky::py {

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
        return PythonEngine::Get()->Init();
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