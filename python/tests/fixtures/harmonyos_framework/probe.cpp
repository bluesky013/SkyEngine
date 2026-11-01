#include <framework/application/GameApplication.h>
#include <framework/platform/OHOSHost.h>

#include <memory>

extern "C" bool
SkyHarmonyOSFrameworkProbe(sky::OHOSApplicationInfo *info, OH_NativeXComponent *component, OHNativeWindow *surface, uint32_t width, uint32_t height)
{
    if (info == nullptr) {
        return false;
    }
    auto *host = sky::OHOSHost::Get();
    if (!host->Configure(*info, []() -> std::unique_ptr<sky::Application> {
            auto application = std::make_unique<sky::GameApplication>();
            application->SetExternalFrameLoop(true);
            if (!application->Init(0, nullptr)) {
                return {};
            }
            return application;
        })) {
        host->Stop();
        return false;
    }
    const bool bound    = host->Bind(component);
    const bool active   = host->SetActive(true);
    const bool attached = bound && host->SetSurface(component, surface, width, height);
    const auto status   = host->GetStatus();
    host->Stop();
    return active && attached && status != sky::OHOSHostStatus::FAILED && status != sky::OHOSHostStatus::END;
}
