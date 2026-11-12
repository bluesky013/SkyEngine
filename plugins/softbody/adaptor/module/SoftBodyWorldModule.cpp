#include <animation/IAnimationPoseContributor.h>
#include <framework/interface/IModule.h>
#include <framework/interface/Interface.h>
#include <softbody/world/SoftBodyComponent.h>

namespace sky::softbody {
    class SoftBodyWorldModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            SoftBodyComponent::Reflect(SerializationContext::Get());
        }
        bool Init(const StartArguments &) override
        {
            if (started)
                return true;
            if (!Interface<IAnimationInstanceControl>::Get()->GetApi() || !Interface<IAnimationPoseContributionRegistry>::Get()->GetApi())
                return false;
            started = StartSoftBodyWorldControl();
            return started;
        }
        void Shutdown() override
        {
            if (started) {
                StopSoftBodyWorldControl();
                started = false;
            }
        }

    private:
        bool started = false;
    };
} // namespace sky::softbody
REGISTER_MODULE_TAGGED(sky::softbody::SoftBodyWorldModule)
