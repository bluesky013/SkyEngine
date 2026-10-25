//
// Content runtime service; schema and handlers belong to Content.Metadata.
//

#include <content/adaptor/AnimationClipSource.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::content {

    class ContentModule : public IModule {
    public:
        ContentModule()           = default;
        ~ContentModule() override = default;

        bool Init(const StartArguments &) override
        {
            auto *service = Interface<IAnimationClipSource>::Get();
            if (source) {
                return service->GetApi() == source.get();
            }
            if (service->GetApi()) {
                return false;
            }
            const auto *system  = Interface<ISystemNotify>::Get()->GetApi();
            auto       *modules = system ? system->GetModuleManager() : nullptr;
            source              = CreateAnimationClipSource(modules ? modules->RetainModule("Content.Module") : nullptr);
            if (!source) {
                return false;
            }
            service->Register(*source);
            return true;
        }
        void Shutdown() override
        {
            auto *service = Interface<IAnimationClipSource>::Get();
            if (source && service->GetApi() == source.get()) {
                service->UnRegister();
            }
            source.reset();
        }

    private:
        std::unique_ptr<IAnimationClipSource> source;
    };

} // namespace sky::content
REGISTER_MODULE_TAGGED(sky::content::ContentModule)
