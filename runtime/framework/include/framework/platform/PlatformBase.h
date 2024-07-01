//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <functional>
#include <core/environment/Singleton.h>
#include <core/platform/Platform.h>
#include <core/file/FileSystem.h>
#include <framework/performance/AdaptivePerfManager.h>

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
        virtual std::string GetBundlePath() const = 0;
        virtual void *GetMainWinHandle() const { return nullptr; };
        virtual void *GetNativeApp() const { return nullptr; }
        virtual std::string GetEnvVariable(const std::string &env) const { return ""; }
        virtual AdaptivePerfManager *GetPerformanceManager() const { return nullptr; }
        virtual bool RunCmd(const std::string &str, std::string &out) const { return true; }
        virtual PlatformType GetType() const { return PlatformType::UNDEFINED; }
        virtual FileSystemPtr GetBundleFileSystem() { return nullptr; }
        virtual char* GetClipBoardText() { return nullptr; }
        virtual void FreeClipBoardText(char* text) {}
        virtual void SetClipBoardText(const std::string &text) {}
        virtual void PollEvent(bool &exit) {}
        void setLaunchCallback(LaunchCallback &&cb) { launchCallback = std::move(cb); }
    protected:
        LaunchCallback launchCallback;
    };

    class Platform : public Singleton<Platform> {
    public:
        bool Init(const PlatformInfo&);
        void Shutdown();

        static std::string GetPlatformNameByType(PlatformType type);
        static PlatformType GetPlatformTypeByName(const std::string &name);

        uint64_t GetPerformanceFrequency() const;
        uint64_t GetPerformanceCounter() const;
        std::string GetInternalPath() const;
        std::string GetBundlePath() const;

        void *GetMainWinHandle() const;
        void *GetNativeApp() const;
        AdaptivePerfManager *GetPerformanceManager() const;
        std::string GetEnvVariable(const std::string &env) const;
        FileSystemPtr GetBundleFileSystem() const;

        char* GetClipBoardText() const;
        void FreeClipBoardText(char* text);
        void SetClipBoardText(const std::string &text);

        bool RunCmd(const std::string &str, std::string &out) const;
        PlatformType GetType() const;

        void PoolEvent(bool &exit);

        template <typename T>
        void setLaunchCallback(T &&cb)
        {
            platform->setLaunchCallback(std::forward<T>(cb));
        }
    private:
        std::unique_ptr<PlatformBase>  platform;
    };
}
