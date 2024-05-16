//
// Created by bluesky on 2023/4/15.
//

#include <framework/platform/PlatformBase.h>

namespace sky {
    void Platform::Shutdown()
    {
        platform = nullptr;
    }

    uint64_t Platform::GetPerformanceFrequency() const
    {
        return platform->GetPerformanceFrequency();
    }

    uint64_t Platform::GetPerformanceCounter() const
    {
        return platform->GetPerformanceCounter();
    }

    std::string Platform::GetInternalPath() const
    {
        return platform->GetInternalPath();
    }

    std::string Platform::GetBundlePath() const
    {
        return platform->GetBundlePath();
    }

    void *Platform::GetMainWinHandle() const
    {
        return platform->GetMainWinHandle();
    }

    void *Platform::GetNativeApp() const
    {
        return platform->GetNativeApp();
    }

    AdaptivePerfManager *Platform::GetPerformanceManager() const
    {
        return platform->GetPerformanceManager();
    }

    std::string Platform::GetEnvVariable(const std::string &env) const
    {
        return platform->GetEnvVariable(env);
    }

    FileSystemPtr Platform::GetBundleFileSystem() const
    {
        return platform->GetBundleFileSystem();
    }

    bool Platform::RunCmd(const std::string &str, std::string &out) const
    {
        return platform->RunCmd(str, out);
    }

    PlatformType Platform::GetType() const
    {
        return platform->GetType();
    }
}
