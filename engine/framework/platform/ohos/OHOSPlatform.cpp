#include "OHOSPlatform.h"
#include "OHOSBundleFileSystem.h"
#include "OHOSDialog.h"
#include "OHOSIME.h"
#include "OHOSWindow.h"

#include <chrono>
#include <cmath>
#include <core/logger/Logger.h>
#include <filesystem>

namespace sky {

    namespace {
        constexpr const char *TAG = "OHOSPlatform";
    } // namespace

    bool Platform::Init(const PlatformInfo &info)
    {
        auto implementation = std::make_unique<OHOSPlatform>();
        if (!implementation->Init(info)) {
            return false;
        }
        platform = std::move(implementation);
        return true;
    }

    bool OHOSPlatform::Init(const PlatformInfo &info)
    {
        if (info.application == nullptr) {
            LOG_E(TAG, "Missing OHOSApplicationInfo");
            return false;
        }
        const auto &application = *static_cast<const OHOSApplicationInfo *>(info.application);
        if (!application.resourceManager || !std::filesystem::path(application.filesPath).is_absolute() ||
            !std::filesystem::path(application.nativeLibraryPath).is_absolute()) {
            LOG_E(TAG, "Resource manager and absolute sandbox/native library paths are required");
            return false;
        }
        context     = application;
        bundle      = CreateOHOSBundleFileSystem(context.resourceManager);
        ownerThread = std::this_thread::get_id();
        return bundle != nullptr;
    }

    OHOSPlatform::~OHOSPlatform()
    {
        if (ownerThread != std::thread::id{}) {
            HideTextInputDialog();
            StopTextInput();
        }
        if (window != nullptr) {
            window->DetachPlatform();
        }
    }

    uint64_t OHOSPlatform::GetPerformanceFrequency() const
    {
        return 1000000000ull;
    }
    uint64_t OHOSPlatform::GetPerformanceCounter() const
    {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    std::string OHOSPlatform::GetInternalPath() const
    {
        return context.filesPath;
    }
    std::string OHOSPlatform::GetBundlePath() const
    {
        return context.nativeLibraryPath;
    }
    std::string OHOSPlatform::GetUserConfigPath() const
    {
        return context.filesPath + '/';
    }
    PlatformType OHOSPlatform::GetType() const
    {
        return PlatformType::HarmonyOS;
    }
    FileSystemPtr OHOSPlatform::GetBundleFileSystem()
    {
        return bundle;
    }
    void *OHOSPlatform::GetMainWinHandle() const
    {
        return const_cast<void *>(surface.get());
    }
    bool OHOSPlatform::IsFrameActive() const
    {
        return active && !exitRequested && surface && width != 0 && height != 0;
    }
    uint64_t OHOSPlatform::GetFrameEpoch() const
    {
        return epoch;
    }
    bool OHOSPlatform::IsOwnerThread() const
    {
        return ownerThread == std::this_thread::get_id();
    }
    void OHOSPlatform::PollEvent(bool &exit, EventPollMode)
    {
        PollOHOSTextInputDialog();
        exit = exit || exitRequested;
    }

    bool OHOSPlatform::BindWindow(OHOSWindow *value)
    {
        if (!IsOwnerThread() || window != nullptr || !surface || value == nullptr) {
            return false;
        }
        window = value;
        window->SetSurface(surface, width, height, scale, false);
        UpdateTextInput();
        return true;
    }

    void OHOSPlatform::UnbindWindow(OHOSWindow *value)
    {
        if (window == value) {
            window->CancelInput();
            window = nullptr;
            UpdateTextInput();
        }
    }

    bool OHOSPlatform::SetSurface(std::shared_ptr<const void> handle, uint32_t newWidth, uint32_t newHeight, float dpi)
    {
        if (!IsOwnerThread() || !std::isfinite(dpi) || dpi <= 0.0f) {
            return false;
        }
        if (!handle || newWidth == 0 || newHeight == 0 || handle.get() != surface.get()) {
            HideTextInputDialog();
        }
        auto previous = std::move(surface);
        surface       = std::move(handle);
        width         = surface != nullptr ? newWidth : 0;
        height        = surface != nullptr ? newHeight : 0;
        scale         = dpi;
        ++epoch;
        if (window != nullptr) {
            window->SetSurface(surface, width, height, scale, true);
        }
        UpdateTextInput();
        return true;
    }

    bool OHOSPlatform::SetActive(bool value)
    {
        if (!IsOwnerThread()) {
            return false;
        }
        if (active != value) {
            active = value;
            ++epoch;
            if (!active && window != nullptr) {
                HideTextInputDialog();
                window->CancelInput();
            }
            UpdateTextInput();
        }
        return true;
    }

    bool OHOSPlatform::SetFocus(bool focus)
    {
        if (!IsOwnerThread()) {
            return false;
        }
        if (focused != focus) {
            focused = focus;
            if (window != nullptr) {
                if (!focused) {
                    window->CancelInput();
                }
                Event<IWindowEvent>::BroadCast(window, &IWindowEvent::OnFocusChanged, focused);
            }
            UpdateTextInput();
        }
        return true;
    }

    OHOSWindow *OHOSPlatform::GetInputWindow() const
    {
        return IsOwnerThread() && IsFrameActive() && focused ? window : nullptr;
    }

    void OHOSPlatform::StartTextInput()
    {
        if (!IsOwnerThread()) {
            LOG_E(TAG, "Text input requires the engine owner thread");
            return;
        }
        textInputRequested = true;
        if (textInputStarted && GetInputWindow() != nullptr) {
            if (!OHOSIME::Get()->ShowKeyboard()) {
                LOG_E(TAG, "Native IME keyboard display failed");
            }
            return;
        }
        UpdateTextInput();
    }

    void OHOSPlatform::StopTextInput()
    {
        if (!IsOwnerThread()) {
            LOG_E(TAG, "Text input requires the engine owner thread");
            return;
        }
        textInputRequested = false;
        UpdateTextInput();
    }

    void OHOSPlatform::SetTextInputRect(int32_t x, int32_t y, int32_t width, int32_t height)
    {
        if (!IsOwnerThread() || width < 0 || height < 0) {
            LOG_E(TAG, "Invalid text input rectangle or owner thread");
            return;
        }
        textInputRect[0] = x;
        textInputRect[1] = y;
        textInputRect[2] = width;
        textInputRect[3] = height;
        if (textInputStarted && !OHOSHost::Get()->SetTextInputRect(x, y, width, height)) {
            LOG_E(TAG, "Failed to update the native IME cursor rectangle");
        }
    }

    void OHOSPlatform::UpdateTextInput()
    {
        if (textInputRequested && GetInputWindow() != nullptr) {
            if (!textInputStarted) {
                auto *host = OHOSHost::Get();
                textInputStarted =
                    host->SetTextInputRect(textInputRect[0], textInputRect[1], textInputRect[2], textInputRect[3]) && host->StartTextInput();
                if (!textInputStarted) {
                    LOG_E(TAG, "Native IME attachment failed");
                }
            }
        } else {
            if (!OHOSHost::Get()->StopTextInput()) {
                LOG_E(TAG, "Native IME detachment failed");
            }
            textInputStarted = false;
        }
        if (window != nullptr) {
            window->SetTextInputActive(textInputStarted);
        }
    }

    bool OHOSPlatform::ShowTextInputDialog(const NativeTextInputOptions &options, NativeTextInputCallback callback)
    {
        if (!IsOwnerThread() || !IsFrameActive() || window == nullptr)
            return false;
        StopTextInput();
        return ShowOHOSTextInputDialog(options, std::move(callback));
    }

    void OHOSPlatform::HideTextInputDialog()
    {
        if (IsOwnerThread())
            HideOHOSTextInputDialog();
    }

    bool OHOSPlatform::RequestExit()
    {
        if (!IsOwnerThread()) {
            return false;
        }
        exitRequested = true;
        active        = false;
        ++epoch;
        return true;
    }

} // namespace sky
