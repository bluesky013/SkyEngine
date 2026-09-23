//
// Native Cocoa platform (no SDL).
//

#pragma once

#include <framework/platform/PlatformBase.h>

namespace sky {

    class CocoaWindow;

    // Native macOS platform (no SDL): NSApplication event pump, clipboard,
    // mach timing, paths, and file dialogs. GetMainWinHandle() returns the
    // main window's CAMetalLayer (the RHI surface handle on macOS).
    class MacosPlatform : public PlatformBase {
    public:
        MacosPlatform() = default;
        ~MacosPlatform() override = default;

        bool Init(const PlatformInfo &info) override;
        uint64_t GetPerformanceFrequency() const override;
        uint64_t GetPerformanceCounter() const override;
        std::string GetInternalPath() const override;
        std::string GetBundlePath() const override;
        std::string GetUserConfigPath() const override;
        void *GetMainWinHandle() const override { return mainWindow; }
        std::string GetEnvVariable(const std::string &env) const override;
        bool RunCmd(const std::string &str, std::string &out) const override;
        PlatformType GetType() const override { return PlatformType::MacOS; }

        char* GetClipBoardText() override;
        void FreeClipBoardText(char* text) override;
        void SetClipBoardText(const std::string &text) override;

        void PollEvent(bool &exit) override;

        bool ShowOpenFileDialog(void *owner, std::string &outPath, const std::string &title,
                                const std::string &filter) override;
        bool ShowSaveFileDialog(void *owner, std::string &outPath, const std::string &title,
                                const std::string &filter) override;

        // Records the first created window's layer as the main surface (used by
        // the RHI when a host does not pass a handle explicitly).
        void SetMainWindow(void *handle)
        {
            if (mainWindow == nullptr) {
                mainWindow = handle;
            }
        }

        // Called by CocoaWindow on close; closing the main window exits the loop.
        void NotifyWindowClosed(void *handle)
        {
            if (handle == mainWindow) {
                exitRequested = true;
            }
        }

    private:
        void *mainWindow    = nullptr; // CAMetalLayer of the main window
        bool  exitRequested = false;
    };

} // namespace sky
