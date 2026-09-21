//
// Audio builder module: registers the audio asset builder with the framework
// offline builder. Owned by the audio subsystem, not by any renderer.
//

#include <builder/audio/AudioBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>

namespace sky::builder {

    class AudioBuilderModule : public IModule {
    public:
        AudioBuilderModule()           = default;
        ~AudioBuilderModule() override = default;

        bool Init(const StartArguments &args) override
        {
            AssetBuilderManager::Get()->RegisterBuilder(new AudioBuilder());
            return true;
        }

        void Tick(float delta) override {}
    };

} // namespace sky::builder
REGISTER_MODULE(sky::builder::AudioBuilderModule)
