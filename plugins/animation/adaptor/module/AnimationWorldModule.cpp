#include <animation/IAnimationClipSource.h>
#include <animation/IAnimationSkeletonSource.h>
#include <animation/world/AnimationComponent.h>
#include <animation/world/AnimationWorldHost.h>
#include <framework/application/ModuleManager.h>
#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/world/WorldSubSystemRegistry.h>

namespace sky::animation {
    class AnimationWorldModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            AnimationComponent::Reflect(SerializationContext::Get());
        }
        bool Init(const StartArguments &) override
        {
            if (host) {
                return true;
            }
            if (Interface<IAnimationInstanceControl>::Get()->GetApi() || Interface<aurora::ISkeletalPoseSource>::Get()->GetApi() ||
                WorldSubSystemRegistry::Get().IsRegistered(Name(ANIMATION_WORLD_SYSTEM)) || !Interface<IAnimationClipSource>::Get()->GetApi() ||
                !Interface<IAnimationSkeletonSource>::Get()->GetApi()) {
                return false;
            }
            const auto *system  = Interface<ISystemNotify>::Get()->GetApi();
            auto       *modules = system ? system->GetModuleManager() : nullptr;
            host                = CreateAnimationWorldHost(modules ? modules->RetainModule("Animation.World") : nullptr);
            if (!host) {
                return false;
            }
            WorldSubSystemRegistration registration;
            registration.moduleLease = modules->RetainModule("Animation.World");
            registration.factory     = [this](World &, const Any &) { return host->CreateSystem(); };
            if (!WorldSubSystemRegistry::Get().Register(Name(ANIMATION_WORLD_SYSTEM), std::move(registration))) {
                host.reset();
                return false;
            }
            Interface<IAnimationInstanceControl>::Get()->Register(*host);
            Interface<aurora::ISkeletalPoseSource>::Get()->Register(*host);
            return true;
        }
        void Shutdown() override
        {
            if (!host) {
                return;
            }
            WorldSubSystemRegistry::Get().Unregister(Name(ANIMATION_WORLD_SYSTEM));
            if (Interface<IAnimationInstanceControl>::Get()->GetApi() == host.get()) {
                Interface<IAnimationInstanceControl>::Get()->UnRegister();
            }
            if (Interface<aurora::ISkeletalPoseSource>::Get()->GetApi() == host.get()) {
                Interface<aurora::ISkeletalPoseSource>::Get()->UnRegister();
            }
            host->Drain();
            host.reset();
        }

    private:
        std::unique_ptr<AnimationWorldHost> host;
    };
} // namespace sky::animation
REGISTER_MODULE_TAGGED(sky::animation::AnimationWorldModule)
