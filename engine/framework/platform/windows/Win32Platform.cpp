//
// Created by Zach Lee on 2022/9/25.
//
// Native Win32 platform implementation (no SDL).
//

#include "Win32Platform.h"
#include "Win32Window.h"

#include <core/logger/Logger.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <windows.h>
#include <commdlg.h>
#include <shlobj_core.h>

static const char *TAG = "Win32Platform";

namespace sky {

    namespace {

        std::wstring UTF8ToWide(const std::string &utf8Text)
        {
            if (utf8Text.empty()) {
                return {};
            }
            const int wideLength =
                ::MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), static_cast<int>(utf8Text.size()), nullptr, 0);
            if (wideLength == 0) {
                return {};
            }
            std::wstring wideText(static_cast<size_t>(wideLength), 0);
            ::MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), static_cast<int>(utf8Text.size()),
                                  wideText.data(), wideLength);
            return wideText;
        }

        std::string WideToUTF8(const std::wstring &wideText)
        {
            if (wideText.empty()) {
                return {};
            }
            const int narrowLength = ::WideCharToMultiByte(
                CP_UTF8, 0, wideText.data(), static_cast<int>(wideText.size()), nullptr, 0, nullptr, nullptr);
            if (narrowLength == 0) {
                return {};
            }
            std::string narrowText(static_cast<size_t>(narrowLength), 0);
            ::WideCharToMultiByte(CP_UTF8, 0, wideText.data(), static_cast<int>(wideText.size()), narrowText.data(),
                                  narrowLength, nullptr, nullptr);
            return narrowText;
        }

        // Builds a double-null-terminated Win32 filter string.
        std::wstring BuildDialogFilter(const std::string &filter)
        {
            std::wstring result = L"All Files\0*.*\0";
            if (!filter.empty()) {
                std::wstring pattern = UTF8ToWide(filter);
                result += pattern;
                result += L'\0';
                result += pattern;
                result += L'\0';
            }
            result += L'\0';
            return result;
        }

    } // namespace

    bool Platform::Init(const PlatformInfo &info)
    {
        platform = std::make_unique<Win32Platform>();
        return platform->Init(info);
    }

    bool Win32Platform::Init(const PlatformInfo & /*info*/)
    {
        // Pre-create the window class so window creation cannot fail on it later.
        return Win32Window::EnsureWindowClass();
    }

    PlatformType Win32Platform::GetType() const
    {
        return PlatformType::Windows;
    }

    uint64_t Win32Platform::GetPerformanceFrequency() const
    {
        LARGE_INTEGER frequency = {};
        ::QueryPerformanceFrequency(&frequency);
        return static_cast<uint64_t>(frequency.QuadPart);
    }

    uint64_t Win32Platform::GetPerformanceCounter() const
    {
        LARGE_INTEGER counter = {};
        ::QueryPerformanceCounter(&counter);
        return static_cast<uint64_t>(counter.QuadPart);
    }

    std::string Win32Platform::GetInternalPath() const
    {
        wchar_t fullPath[MAX_PATH + 1] = {0};
        ::GetModuleFileNameW(nullptr, fullPath, MAX_PATH + 1);
        return std::filesystem::path(WideToUTF8(fullPath)).parent_path().string();
    }

    std::string Win32Platform::GetBundlePath() const
    {
        return GetInternalPath(); // TODO
    }

    std::string Win32Platform::GetUserConfigPath() const
    {
        PWSTR path = nullptr;
        if (::SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path) != S_OK || path == nullptr) {
            return {};
        }
        const std::filesystem::path configDir =
            std::filesystem::path(path) / L"SkyEngine" / L"SkyEditor";
        ::CoTaskMemFree(path);

        std::error_code ec;
        std::filesystem::create_directories(configDir, ec);

        std::string result = configDir.string();
        if (!result.empty() && result.back() != '\\' && result.back() != '/') {
            result += '\\';
        }
        return result;
    }

    std::string Win32Platform::GetEnvVariable(const std::string &name) const
    {
        const std::wstring envKey = UTF8ToWide(name);
        const DWORD size = ::GetEnvironmentVariableW(envKey.c_str(), nullptr, 0);
        if (size == 0 || size == 1) {
            return {};
        }
        std::wstring envValue(size, 0);
        const DWORD length = ::GetEnvironmentVariableW(envKey.data(), envValue.data(), size);
        if (length == 0 || length >= size) {
            return {};
        }
        envValue.resize(length);
        return WideToUTF8(envValue);
    }

    bool Win32Platform::RunCmd(const std::string &cmd, std::string &out) const
    {
        FILE *pipe = _popen(cmd.c_str(), "r");
        if (pipe == nullptr) {
            return false;
        }
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            out += buffer;
        }
        _pclose(pipe);
        return true;
    }

    char* Win32Platform::GetClipBoardText()
    {
        if (::OpenClipboard(nullptr) == FALSE) {
            return nullptr;
        }
        char *result = nullptr;
        if (HANDLE handle = ::GetClipboardData(CF_UNICODETEXT)) {
            auto *wide = static_cast<const wchar_t *>(::GlobalLock(handle));
            if (wide != nullptr) {
                const std::string utf8 = WideToUTF8(wide);
                result = static_cast<char *>(std::malloc(utf8.size() + 1));
                if (result != nullptr) {
                    std::memcpy(result, utf8.c_str(), utf8.size() + 1);
                }
                ::GlobalUnlock(handle);
            }
        }
        ::CloseClipboard();
        return result;
    }

    void Win32Platform::FreeClipBoardText(char *text)
    {
        std::free(text);
    }

    void Win32Platform::SetClipBoardText(const std::string &text)
    {
        const std::wstring wide = UTF8ToWide(text);
        const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);

        if (::OpenClipboard(nullptr) == FALSE) {
            return;
        }
        ::EmptyClipboard();
        if (HGLOBAL handle = ::GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (void *dest = ::GlobalLock(handle)) {
                std::memcpy(dest, wide.c_str(), bytes);
                ::GlobalUnlock(handle);
                ::SetClipboardData(CF_UNICODETEXT, handle);
            }
        }
        ::CloseClipboard();
    }

    void Win32Platform::PollEvent(bool &exit)
    {
        MSG msg;
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                exit = true;
                continue;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }

    bool Win32Platform::ShowOpenFileDialog(void *owner, std::string &outPath, const std::string &title,
                                           const std::string &filter)
    {
        wchar_t fileName[MAX_PATH] = {0};
        const std::wstring wideTitle = UTF8ToWide(title);
        const std::wstring wideFilter = BuildDialogFilter(filter);

        OPENFILENAMEW ofn = {};
        ofn.lStructSize  = sizeof(ofn);
        ofn.hwndOwner    = static_cast<HWND>(owner);
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrTitle   = wideTitle.empty() ? nullptr : wideTitle.c_str();
        ofn.lpstrFilter  = wideFilter.c_str();
        ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

        if (::GetOpenFileNameW(&ofn) == TRUE) {
            outPath = WideToUTF8(fileName);
            return true;
        }
        return false;
    }

    bool Win32Platform::ShowSaveFileDialog(void *owner, std::string &outPath, const std::string &title,
                                           const std::string &filter)
    {
        wchar_t fileName[MAX_PATH] = {0};
        const std::wstring wideTitle = UTF8ToWide(title);
        const std::wstring wideFilter = BuildDialogFilter(filter);

        OPENFILENAMEW ofn = {};
        ofn.lStructSize  = sizeof(ofn);
        ofn.hwndOwner    = static_cast<HWND>(owner);
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrTitle   = wideTitle.empty() ? nullptr : wideTitle.c_str();
        ofn.lpstrFilter  = wideFilter.c_str();
        ofn.Flags        = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_EXPLORER;

        if (::GetSaveFileNameW(&ofn) == TRUE) {
            outPath = WideToUTF8(fileName);
            return true;
        }
        return false;
    }

} // namespace sky
