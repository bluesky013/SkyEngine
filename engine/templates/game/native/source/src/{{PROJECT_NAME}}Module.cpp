//
// {{PROJECT_NAME}} game module entry.
//

#include <{{PROJECT_NAME}}Module.h>

#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

#include <core/logger/Logger.h>

static const char *TAG = "{{PROJECT_NAME}}";

namespace sky {

    bool {{PROJECT_NAME}}Module::Init(const StartArguments &args)
    {
        (void)args;
        LOG_I(TAG, "{{PROJECT_NAME}} module init");
        // Register game components / serialization types here, e.g.
        // SomeComponent::Reflect(SerializationContext::Get());
        return true;
    }

    void {{PROJECT_NAME}}Module::Start()
    {
    }

    void {{PROJECT_NAME}}Module::Tick(float delta)
    {
        (void)delta;
    }

    void {{PROJECT_NAME}}Module::Shutdown()
    {
    }

} // namespace sky

REGISTER_MODULE_TAGGED(sky::{{PROJECT_NAME}}Module)
