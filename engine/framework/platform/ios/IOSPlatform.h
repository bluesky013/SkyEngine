//
// Native UIKit platform for iOS.
//

#pragma once

#include <framework/platform/PlatformBase.h>

namespace sky {

    // Native iOS platform (UIKit): app lifecycle, clipboard, mach timing, paths.
    // There is no NSApplication-style loop: UIKit owns the run loop and the frame
    // loop is driven by a CADisplayLink (see IOSWindow). GetMainWinHandle() returns
    // the main window's CAMetalLayer (the RHI surface handle on iOS).
    class IOSPlatform : public PlatformBase {
    public:
        IOSPlatform()           = default;
        ~IOSPlatform() override = default;

        bool Init(const PlatformInfo &info) override;
        uint64_t GetPerformanceFrequency() const override;
        uint64_t GetPerformanceCounter() const override;
        std::string GetInternalPath() const override;
        std::string GetBundlePath() const override;
        std::string GetUserConfigPath() const override;
        void *GetMainWinHandle() const override { return mainWindow; }
        void *GetNativeApp() const override { return nativeApp; }
        std::string GetEnvVariable(const std::string &env) const override;
        bool RunCmd(const std::string &str, std::string &out) const override;
        PlatformType GetType() const override { return PlatformType::IOS; }

        char* GetClipBoardText() override;
        void FreeClipBoardText(char* text) override;
        void SetClipBoardText(const std::string &text) override;

        void PollEvent(bool &exit) override;

        // Records the first created window's layer as the main surface.
        void SetMainWindow(void *handle)
        {
            if (mainWindow == nullptr) {
                mainWindow = handle;
            }
        }

    private:
        void *mainWindow = nullptr; // CAMetalLayer of the main window
        void *nativeApp  = nullptr; // UIApplication
    };

} // namespace sky
