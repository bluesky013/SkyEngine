#pragma once

#include <core/environment/Singleton.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <native_window/external_window.h>
#include <rawfile/raw_file_manager.h>
#include <string>

struct OH_NativeXComponent;
struct ArkUI_UIInputEvent;

namespace sky {

    class Application;

    // Pass through PlatformInfo::application; Platform::Init copies this descriptor.
    struct OHOSApplicationInfo {
        std::string                            filesPath;
        std::string                            nativeLibraryPath;
        std::shared_ptr<NativeResourceManager> resourceManager;
    };

    enum class OHOSHostStatus : int32_t { NEW, WAITING_FOR_SURFACE, RUNNING, FAILED, STOPPED, END };
    using OHOSApplicationFactory = std::function<std::unique_ptr<Application>()>;

    // The process-owned receiver survives sessions and late VSync callbacks; use Stop, not Destroy.
    class OHOSHost final : public Singleton<OHOSHost> {
    public:
        ~OHOSHost() override;

        // Invoked on the engine thread once an active surface is usable.
        // Return an initialized application using an external frame loop, or nullptr on failure.
        bool Configure(OHOSApplicationInfo info, OHOSApplicationFactory factory);
        bool Bind(OH_NativeXComponent *component);
        // The source must be the bound XComponent; nullptr surface reports its destruction and unbinds it.
        bool SetSurface(OH_NativeXComponent *source, OHNativeWindow *surface, uint32_t width, uint32_t height);
        bool SetActive(bool active);
        bool SetFocus(OH_NativeXComponent *source, bool focused);
        bool DispatchTouch(OH_NativeXComponent *source, OHNativeWindow *surface);
        bool DispatchMouse(OH_NativeXComponent *source, OHNativeWindow *surface);
        bool DispatchKey(OH_NativeXComponent *source, OHNativeWindow *surface);
        bool DispatchAxis(OH_NativeXComponent *source, ArkUI_UIInputEvent *event);
        bool StartTextInput(uint32_t windowId = 0);
        bool StopTextInput();
        bool SetTextInputRect(int32_t x, int32_t y, int32_t width, int32_t height);
        void NotifyTextInputDialog();
        // Called by the external N-API owner; joins the engine thread before releasing session resources.
        void           Stop();
        OHOSHostStatus GetStatus() const;

    private:
        friend class Singleton<OHOSHost>;
        using Singleton<OHOSHost>::Destroy;
        OHOSHost();
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

} // namespace sky
