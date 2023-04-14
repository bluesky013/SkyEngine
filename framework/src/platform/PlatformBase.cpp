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

    void *Platform::GetMainWinHandle() const
    {
        return platform->GetMainWinHandle();
    }
}