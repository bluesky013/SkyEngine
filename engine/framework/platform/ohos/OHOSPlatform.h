#pragma once

#include <framework/platform/OHOSHost.h>
#include <framework/platform/PlatformBase.h>
#include <thread>

namespace sky {

    class OHOSWindow;

    class OHOSPlatform final : public PlatformBase {
    public:
        ~OHOSPlatform() override;
        bool          Init(const PlatformInfo &info) override;
        uint64_t      GetPerformanceFrequency() const override;
        uint64_t      GetPerformanceCounter() const override;
        std::string   GetInternalPath() const override;
        std::string   GetBundlePath() const override;
        std::string   GetUserConfigPath() const override;
        PlatformType  GetType() const override;
        FileSystemPtr GetBundleFileSystem() override;
        void         *GetMainWinHandle() const override;
        bool          IsFrameActive() const override;
        uint64_t      GetFrameEpoch() const override;
        void          PollEvent(bool &exit, EventPollMode mode) override;
        void          StartTextInput() override;
        void          StopTextInput() override;
        void          SetTextInputRect(int32_t x, int32_t y, int32_t width, int32_t height) override;
        bool          ShowTextInputDialog(const NativeTextInputOptions &options, NativeTextInputCallback callback) override;
        void          HideTextInputDialog() override;

        bool        SetSurface(std::shared_ptr<const void> handle, uint32_t width, uint32_t height, float scale);
        bool        SetActive(bool active);
        bool        SetFocus(bool focus);
        OHOSWindow *GetInputWindow() const;
        bool        RequestExit();
        bool        IsOwnerThread() const;
        bool        BindWindow(OHOSWindow *window);
        void        UnbindWindow(OHOSWindow *window);

    private:
        void                        UpdateTextInput();
        OHOSApplicationInfo         context;
        FileSystemPtr               bundle;
        std::shared_ptr<const void> surface;
        OHOSWindow                 *window = nullptr;
        std::thread::id             ownerThread;
        uint32_t                    width              = 0;
        uint32_t                    height             = 0;
        float                       scale              = 1.0f;
        uint64_t                    epoch              = 0;
        bool                        active             = false;
        bool                        exitRequested      = false;
        bool                        focused            = false;
        bool                        textInputRequested = false;
        bool                        textInputStarted   = false;
        int32_t                     textInputRect[4]   = {};
    };

} // namespace sky
