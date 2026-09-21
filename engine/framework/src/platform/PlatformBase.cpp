//
// Created by bluesky on 2023/4/15.
//

#include <framework/platform/PlatformBase.h>

namespace sky {

    std::string Platform::GetPlatformNameByType(PlatformType type)
    {
        switch (type) {
            case PlatformType::Windows:
                return "Windows";
            case PlatformType::MacOS:
                return "MacOS";
            case PlatformType::Linux:
                return "Linux";
            case PlatformType::Android:
                return "Android";
            case PlatformType::IOS:
                return "iOS";
            case PlatformType::Default:
            case PlatformType::UNDEFINED:
                break;
        }
        return {};
    }

    PlatformType Platform::GetPlatformTypeByName(const std::string &name)
    {
        if (name == "Windows") {
            return PlatformType::Windows;
        }
        if (name == "Linux") {
            return PlatformType::Linux;
        }
        if (name == "MacOS") {
            return PlatformType::MacOS;
        }
        if (name == "Android") {
            return PlatformType::Android;
        }
        if (name == "iOS") {
            return PlatformType::IOS;
        }
        return PlatformType::UNDEFINED;
    }

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

    std::string Platform::GetUserConfigPath() const
    {
        return platform != nullptr ? platform->GetUserConfigPath() : std::string{};
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

    char* Platform::GetClipBoardText() const
    {
        return platform->GetClipBoardText();
    }

    void Platform::FreeClipBoardText(char* text)
    {
        platform->FreeClipBoardText(text);
    }

    void Platform::SetClipBoardText(const std::string &text)
    {
        platform->SetClipBoardText(text);
    }

    bool Platform::ShowOpenFileDialog(void *owner, std::string &outPath, const std::string &title,
                                      const std::string &filter) const
    {
        return platform->ShowOpenFileDialog(owner, outPath, title, filter);
    }

    bool Platform::ShowSaveFileDialog(void *owner, std::string &outPath, const std::string &title,
                                      const std::string &filter) const
    {
        return platform->ShowSaveFileDialog(owner, outPath, title, filter);
    }

    void Platform::StartTextInput()
    {
        platform->StartTextInput();
    }

    void Platform::StopTextInput()
    {
        platform->StopTextInput();
    }

    void Platform::SetTextInputRect(int32_t x, int32_t y, int32_t width, int32_t height)
    {
        platform->SetTextInputRect(x, y, width, height);
    }

    void Platform::PoolEvent(bool &exit)
    {
        platform->PollEvent(exit);
    }
}
