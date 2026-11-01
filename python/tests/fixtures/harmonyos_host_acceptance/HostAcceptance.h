#pragma once

#include <framework/platform/OHOSHost.h>
#include <string>
#include <vector>

namespace host_acceptance {
    struct Surface {
        OH_NativeXComponent *component = nullptr;
        OHNativeWindow      *window    = nullptr;
        uint32_t             width     = 0;
        uint32_t             height    = 0;
    };

    // Surfaces must remain SDK-retained by the caller for this synchronous run.
    std::string Run(const sky::OHOSApplicationInfo &info, const std::vector<Surface> &surfaces);
    bool        StartStress(const sky::OHOSApplicationInfo &info);
    void        StressSurfaceChanged(const Surface &surface);
    void        StressSurfaceDestroyed(OH_NativeXComponent *component);
    void        StressActive(bool active);
    std::string StressStats();
    std::string StopStress();
    bool        RetainFixtureSurface(OHNativeWindow *window);
    bool        ReleaseFixtureSurface(OHNativeWindow *window);
} // namespace host_acceptance
