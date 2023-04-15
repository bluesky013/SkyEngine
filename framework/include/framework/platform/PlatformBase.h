//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <functional>
#include <core/environment/Singleton.h>

namespace sky {

    struct PlatformInfo {
        void* application = nullptr;
    };

    using LaunchCallback = std::function<void()>;

    class PlatformBase {
    public:
        PlatformBase() = default;
        virtual ~PlatformBase() = default;

        virtual bool Init(const PlatformInfo &info) = 0;
        virtual uint64_t GetPerformanceFrequency() const = 0;
        virtual uint64_t GetPerformanceCounter() const = 0;
        virtual std::string GetInternalPath() const = 0;
        virtual void *GetMainWinHandle() const { return nullptr; };
        virtual void *GetNativeApp() const { return nullptr; }
        void setLaunchCallback(LaunchCallback &&cb) { launchCallback = std::move(cb); }

    protected:
        LaunchCallback launchCallback;
    };

    class Platform : public Singleton<Platform> {
    public:
        bool Init(const PlatformInfo&);
        void Shutdown();

        uint64_t GetPerformanceFrequency() const;
        uint64_t GetPerformanceCounter() const;
        std::string GetInternalPath() const;
        void *GetMainWinHandle() const;
        void *GetNativeApp() const;

        template <typename T>
        void setLaunchCallback(T &&cb)
        {
            platform->setLaunchCallback(std::forward<T>(cb));
        }
    private:
        std::unique_ptr<PlatformBase> platform;
    };
}
