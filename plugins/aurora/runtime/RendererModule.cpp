#include "RendererProvider.h"
#include <framework/interface/IModule.h>
#include <framework/interface/Interface.h>

namespace sky::aurora {

    class RendererModule final : public IModule {
    public:
        bool Init(const StartArguments &) override
        {
            // Host-thread only; Environment shares the service across DLLs.
            auto *service = Interface<IRendererProvider>::Get();
            if (provider) {
                return service->GetApi() == provider.get();
            }
            if (service->GetApi() != nullptr) {
                return false;
            }
            provider = CreateRendererProvider();
            service->Register(*provider);
            return true;
        }

        void Shutdown() override
        {
            if (!provider) {
                return;
            }
            // Stop/destroy all renderers before unload; unregister only our own provider.
            auto *service = Interface<IRendererProvider>::Get();
            if (service->GetApi() == provider.get()) {
                service->UnRegister();
            }
            provider.reset();
        }

    private:
        std::unique_ptr<IRendererProvider> provider;
    };

} // namespace sky::aurora

REGISTER_MODULE_TAGGED(sky::aurora::RendererModule)
