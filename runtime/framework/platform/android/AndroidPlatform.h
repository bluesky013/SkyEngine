//
// Created by Zach Lee on 2022/9/26.
//

#pragma once

#include <framework/platform/PlatformBase.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "AndroidPerfManager.h"
#include "AndroidBundleFileSystem.h"

namespace sky {

    class AndroidPlatform : public PlatformBase {
    public:
        AndroidPlatform() = default;
        ~AndroidPlatform() = default;

        void SetMainWinHandle(ANativeWindow *handle);

        bool IsLaunched() const;
        void Launch();
    private:
        bool Init(const PlatformInfo& desc) override;

        uint64_t GetPerformanceFrequency() const override;
        uint64_t GetPerformanceCounter() const override;
        std::string GetInternalPath() const override;
        std::string GetBundlePath() const override;
        void *GetMainWinHandle() const override;
        void *GetNativeApp() const override;
        AdaptivePerfManager *GetPerformanceManager() const override;
        FileSystemPtr GetBundleFileSystem() override;

        android_app *app = nullptr;
        ANativeWindow *mainWindow = nullptr;
        bool launched = false;
        std::unique_ptr<AdaptivePerfManager> perfManager;
        std::shared_ptr<AndroidBundleFileSystem> assetFs;
    };
}
