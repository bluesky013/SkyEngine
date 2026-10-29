//
// Native UIKit platform for iOS.
//

#pragma once

#include "IOSPlatformState.h"
#include <framework/platform/PlatformBase.h>

#include <functional>

namespace sky {

    class IOSWindow;

    // Native iOS process services and a single UIWindowScene's frame loop.
    // There is no NSApplication-style loop: UIKit owns the run loop and the frame
    // loop is driven by a CADisplayLink (see IOSWindow). GetMainWinHandle() returns
    // the main window's CAMetalLayer (the RHI surface handle on iOS).
    class IOSPlatform : public PlatformBase {
    public:
        IOSPlatform() = default;
        ~IOSPlatform() override;

        bool          Init(const PlatformInfo &info) override;
        uint64_t      GetPerformanceFrequency() const override;
        uint64_t      GetPerformanceCounter() const override;
        std::string   GetInternalPath() const override;
        std::string   GetBundlePath() const override;
        std::string   GetUserConfigPath() const override;
        FileSystemPtr GetBundleFileSystem() override;
        void         *GetMainWinHandle() const override;
        void         *GetNativeApp() const override
        {
            return nativeApp;
        }
        std::string  GetEnvVariable(const std::string &env) const override;
        bool         RunCmd(const std::string &str, std::string &out) const override;
        PlatformType GetType() const override
        {
            return PlatformType::IOS;
        }

        char *GetClipBoardText() override;
        void  FreeClipBoardText(char *text) override;
        void  SetClipBoardText(const std::string &text) override;

        void     PollEvent(bool &exit, EventPollMode mode = EventPollMode::WAIT_IF_INACTIVE) override;
        bool     IsFrameActive() const override;
        uint64_t GetFrameEpoch() const override
        {
            return frameState.GetEpoch();
        }

        // Frame loop: UIKit owns the run loop, so the host registers a frame
        // callback and starts a CADisplayLink that ticks it each frame.
        void SetFrameCallback(std::function<void()> callback) override;
        void StartFrameLoop() override;
        void StopFrameLoop() override;
        void StartTextInput() override;
        void StopTextInput() override;
        void SetTextInputRect(int32_t x, int32_t y, int32_t width, int32_t height) override;

        // Invoked by the CADisplayLink target.
        void OnDisplayTick();

        bool             RegisterMainWindow(IOSWindow *window);
        void             ClearMainWindow(const IOSWindow *window);
        void             SetWindowSurface(const IOSWindow *window, void *handle);
        const IOSWindow *GetMainWindow() const
        {
            return mainNativeWindow;
        }
        void *GetWindowScene() const
        {
            return windowScene;
        }
        bool ConnectScene(void *scene);
        void SetSceneActive(void *scene, bool active);
        void DisconnectScene(void *scene);

    private:
        void RefreshFrameLoop();
        void InvalidateDisplayLink();

        IOSWindow            *mainNativeWindow = nullptr;
        void                 *nativeApp        = nullptr; // UIApplication
        void                 *displayLink      = nullptr; // CADisplayLink
        void                 *linkTarget       = nullptr; // SkyDisplayLinkTarget
        void                 *windowScene      = nullptr; // UIWindowScene, owned while connected
        ios::FrameState       frameState;
        bool                  textInputEnabled = false;
        int32_t               textInputRect[4] = {0, 0, 1, 1};
        FileSystemPtr         bundleFileSystem;
        std::function<void()> frameCallback;
    };

} // namespace sky
