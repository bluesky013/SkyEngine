//
// Created by Zach Lee on 2022/9/26.
//

#pragma once

#include "AndroidBundleFileSystem.h"
#include "AndroidInput.h"
#include <framework/platform/PlatformBase.h>

struct android_app;
struct ANativeWindow;

namespace sky {
    class NativeWindow;
    class AndroidWindow;

    class AndroidPlatform : public PlatformBase {
    public:
        AndroidPlatform() = default;
        ~AndroidPlatform() override;

        void          SetMainWinHandle(ANativeWindow *handle);
        void          SetNativeWindow(AndroidWindow *window);
        NativeWindow *GetMainWindow() const;
        void          HandleCommand(int32_t command);

        bool     IsLaunched() const;
        void     Launch();
        bool     IsFrameActive() const override;
        uint64_t GetFrameEpoch() const override;
        void     PollEvent(bool &exit, EventPollMode mode = EventPollMode::WAIT_IF_INACTIVE) override;
        void     StartTextInput() override;
        void     StopTextInput() override;

    private:
        bool Init(const PlatformInfo &desc) override;

        uint64_t             GetPerformanceFrequency() const override;
        uint64_t             GetPerformanceCounter() const override;
        std::string          GetInternalPath() const override;
        std::string          GetBundlePath() const override;
        std::string          GetUserConfigPath() const override;
        std::string          GetEnvVariable(const std::string &name) const override;
        PlatformType         GetType() const override;
        void                *GetMainWinHandle() const override;
        void                *GetNativeApp() const override;
        AdaptivePerfManager *GetPerformanceManager() const override;
        FileSystemPtr        GetBundleFileSystem() override;

        android_app                         *app          = nullptr;
        ANativeWindow                       *mainWindow   = nullptr;
        AndroidWindow                       *nativeWindow = nullptr;
        bool                                 launched     = false;
        std::unique_ptr<AdaptivePerfManager> perfManager;
        CounterPtr<AndroidBundleFileSystem>  assetFs;
        AndroidLifecycleState                lifecycle;
    };
} // namespace sky
