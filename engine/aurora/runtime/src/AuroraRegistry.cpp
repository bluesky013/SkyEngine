//
// Aurora runtime service assembly and module entry points.
//

#include <aurora/adaptor/AnimationSkeletonSource.h>
#include <aurora/adaptor/AuroraModule.h>
#include <core/logger/Logger.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>

namespace sky::aurora {
    class AuroraRuntimeModule final : public IModule {
    public:
        ~AuroraRuntimeModule() override
        {
            Shutdown();
        }

        bool Init(const StartArguments &args) override
        {
            if (initialized)
                return true;
            auto *service = Interface<IAnimationSkeletonSource>::Get();
            if (service->GetApi()) {
                LOG_E("AuroraRuntime", "Skeleton source already has a registration owner");
                return false;
            }
            const auto *system  = Interface<ISystemNotify>::Get()->GetApi();
            auto       *modules = system ? system->GetModuleManager() : ModuleManager::Current();
            skeletonSource      = CreateAnimationSkeletonSource(modules ? modules->RetainModule("Aurora.Render") : nullptr);
            if (!skeletonSource) {
                LOG_E("AuroraRuntime", "Skeleton source requires the owning Aurora.Render module lease");
                return false;
            }
            service->Register(*skeletonSource);
            if (!renderer.Init(args)) {
                Shutdown();
                return false;
            }
            initialized = true;
            return true;
        }

        void Start() override
        {
            renderer.Start();
        }

        void Tick(float delta) override
        {
            renderer.Tick(delta);
        }

        void Shutdown() override
        {
            auto *service = Interface<IAnimationSkeletonSource>::Get();
            if (skeletonSource && service->GetApi() == skeletonSource.get())
                service->UnRegister();
            // Stop new requests before draining the host; outstanding requests retain their own code leases.
            renderer.Shutdown();
            skeletonSource.reset();
            initialized = false;
        }

    private:
        AuroraModule                              renderer;
        std::unique_ptr<IAnimationSkeletonSource> skeletonSource;
        bool                                      initialized = false;
    };
} // namespace sky::aurora

REGISTER_MODULE_TAGGED(sky::aurora::AuroraRuntimeModule)
