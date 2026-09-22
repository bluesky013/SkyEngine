//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <framework/platform/PlatformBase.h>

namespace sky {

    // Native Win32 platform (no SDL): message pump, clipboard, timing, and paths.
    class Win32Platform : public PlatformBase {
    public:
        Win32Platform() = default;
        ~Win32Platform() override = default;

        bool Init(const PlatformInfo &info) override;
        uint64_t GetPerformanceFrequency() const override;
        uint64_t GetPerformanceCounter() const override;
        std::string GetInternalPath() const override;
        std::string GetBundlePath() const override;
        std::string GetUserConfigPath() const override;
        void *GetMainWinHandle() const override { return mainWindow; }
        std::string GetEnvVariable(const std::string &env) const override;
        bool RunCmd(const std::string &str, std::string &out) const override;
        PlatformType GetType() const override;

        char* GetClipBoardText() override;
        void FreeClipBoardText(char* text) override;
        void SetClipBoardText(const std::string &text) override;

        void PollEvent(bool &exit) override;

        bool ShowOpenFileDialog(void *owner, std::string &outPath, const std::string &title,
                                const std::string &filter) override;
        bool ShowSaveFileDialog(void *owner, std::string &outPath, const std::string &title,
                                const std::string &filter) override;

        // Records the first created window as the main window (used by the RHI
        // when a host does not pass a handle explicitly).
        void SetMainWindow(void *handle)
        {
            if (mainWindow == nullptr) {
                mainWindow = handle;
            }
        }

    private:
        void *mainWindow = nullptr;
    };

} // namespace sky
