//
// Native X11 platform (no SDL).
//

#pragma once

#include <framework/platform/PlatformBase.h>

namespace sky {

    // Native Linux platform: X11 event pump, in-process clipboard, steady-clock
    // timing, and executable-derived paths. Window creation lives in LinuxWindow.
    class LinuxPlatform : public PlatformBase {
    public:
        LinuxPlatform()           = default;
        ~LinuxPlatform() override;

        bool        Init(const PlatformInfo &info) override;
        uint64_t    GetPerformanceFrequency() const override;
        uint64_t    GetPerformanceCounter() const override;
        std::string GetInternalPath() const override;
        std::string GetBundlePath() const override;
        std::string GetUserConfigPath() const override;
        void       *GetMainWinHandle() const override
        {
            return mainWindow;
        }
        std::string  GetEnvVariable(const std::string &env) const override;
        bool         RunCmd(const std::string &str, std::string &out) const override;
        PlatformType GetType() const override;

        char *GetClipBoardText() override;
        void  FreeClipBoardText(char *text) override;
        void  SetClipBoardText(const std::string &text) override;

        void PollEvent(bool &exit, EventPollMode mode = EventPollMode::WAIT_IF_INACTIVE) override;

        bool ShowOpenFileDialog(void *owner, std::string &outPath, const std::string &title, const std::string &filter) override;
        bool ShowSaveFileDialog(void *owner, std::string &outPath, const std::string &title, const std::string &filter) override;
        void RevealInFileExplorer(const std::string &path) override;

        // Records the first created window as the main window.
        void SetMainWindow(void *handle)
        {
            if (mainWindow == nullptr) {
                mainWindow = handle;
            }
        }

    private:
        void *mainWindow = nullptr;
        bool  ownsDisplay = false;
    };

} // namespace sky
