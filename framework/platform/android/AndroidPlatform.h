//
// Created by Zach Lee on 2022/9/26.
//

#pragma once

#include <framework/platform/PlatformBase.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace sky {

    class AndroidPlatform : public PlatformBase {
    public:
        AndroidPlatform() = default;
        ~AndroidPlatform() = default;

    private:
        bool Init(const Descriptor& desc) override;
        void Shutdown() override;

        uint64_t GetPerformanceFrequency() const override;

        uint64_t GetPerformanceCounter() const override;

        android_app *app = nullptr;
    };
}
