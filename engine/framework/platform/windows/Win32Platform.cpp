//
// Created by Zach Lee on 2022/9/25.
//

#include "Win32Platform.h"

#include <xstring>
#include <filesystem>

#include <windows.h>
#include <commdlg.h>
#include <shlobj_core.h>

static const char* TAG = "Win32Platform";

namespace sky {

    static std::wstring UTF8ToWide(const std::string& utf8Text)
    {
        if (utf8Text.empty()) {
            return {};
        }

        std::wstring wideText;
        const int wideLength = ::MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), (int)utf8Text.size(), nullptr, 0);
        if (wideLength == 0) {
            return {};
        }

        wideText.resize(wideLength, 0);
        auto* wideString = const_cast<wchar_t*>(wideText.data());
        const int length = ::MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), (int)utf8Text.size(), wideString, wideLength);
        if (length != wideLength) {
            return {};
        }

        return wideText;
    }

    static std::string WideToUTF8(const std::wstring& wideText)
    {
        if (wideText.empty()) {
            return {};
        }

        std::string narrowText;
        int narrowLength = ::WideCharToMultiByte(CP_UTF8, 0, wideText.data(), (int)wideText.size(), nullptr, 0, nullptr, nullptr);
        if (narrowLength == 0) {
            return {};
        }

        narrowText.resize(narrowLength, 0);
        char* narrowString = const_cast<char*>(narrowText.data());
        const int length = ::WideCharToMultiByte(CP_UTF8, 0, wideText.data(), (int)wideText.size(), narrowString, narrowLength, nullptr, nullptr);
        if (length != narrowLength) {
            return {};
        }

        return narrowText;
    }

    // Builds a double-null-terminated Win32 filter string.
    static std::wstring BuildDialogFilter(const std::string &filter)
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

    struct Pipe {
        ~Pipe();
        bool Init();
        void CloseRead();
        void CloseWrite();

        HANDLE read;
        HANDLE write;
    };

    bool Pipe::Init()
    {
        SECURITY_ATTRIBUTES security = {sizeof(SECURITY_ATTRIBUTES)};
        security.bInheritHandle = TRUE;
        security.lpSecurityDescriptor = nullptr;

        return CreatePipe(&read, &write, &security, 0) != 0;
    }

    void Pipe::CloseRead()
    {
        if (read != nullptr) {
            CloseHandle(read);
            read = nullptr;
        }
    }

    void Pipe::CloseWrite()
    {
        if (write != nullptr) {
            CloseHandle(write);
            write = nullptr;
        }
    }

    Pipe::~Pipe()
    {
        CloseRead();
        CloseWrite();
    }

    bool Platform::Init(const PlatformInfo& info)
    {
        platform = std::make_unique<Win32Platform>();
        return platform->Init(info);
    }

    PlatformType Win32Platform::GetType() const
    {
        return PlatformType::Windows;
    }

    std::string Win32Platform::GetInternalPath() const
    {
        static const uint32_t MAX_WRITABLE_PATH = 128;
        wchar_t fullPath[MAX_WRITABLE_PATH + 1];
        ::GetModuleFileNameW(nullptr, fullPath, MAX_WRITABLE_PATH + 1);

        return std::filesystem::path(WideToUTF8(fullPath)).parent_path().string();
    }

    std::string Win32Platform::GetBundlePath() const
    {
        return GetInternalPath(); // TODO
    }

    std::string Win32Platform::GetEnvVariable(const std::string &name) const
    {
        const std::wstring envKey = UTF8ToWide(name);
        const DWORD size = ::GetEnvironmentVariableW(envKey.c_str(), nullptr, 0);
        if (size == 0 || size == 1) {
            return {};
        }
        std::wstring envValue(size, 0);
        const DWORD length = ::GetEnvironmentVariableW(envKey.data(), envValue.data(), (DWORD)envValue.size());
        if ((length == 0) || (length >= envValue.size())) {
            return {};
        }
        envValue.resize(length);
        return WideToUTF8(envValue);
    }

    bool Win32Platform::RunCmd(const std::string &cmd, std::string &out) const
    {
        auto pipe = std::make_unique<Pipe>();
        if (!pipe->Init()) {
            return false;
        }

        PROCESS_INFORMATION procInfo = {};
        STARTUPINFO startInfo = {sizeof(STARTUPINFO)};
        startInfo.hStdOutput = pipe->write;
        startInfo.hStdError = pipe->write;
        startInfo.dwFlags |= STARTF_USESTDHANDLES;

        auto success = CreateProcess(nullptr, (char*)cmd.c_str(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startInfo, &procInfo);
        if (success == 0) {
            return false;
        }

        WaitForSingleObject(procInfo.hProcess, 5000);
        for (;;) {
            DWORD dwRead = 0;
            DWORD dwAvail = 0;

            if (::PeekNamedPipe(pipe->read, nullptr, 0, nullptr, &dwAvail, nullptr) == 0) {
                break;
            }
            if (dwAvail == 0) {
                break;
            }
            std::string tmp(dwAvail, 0);
            if ((::ReadFile(pipe->read, (char*)tmp.data(), dwAvail, &dwRead, nullptr) == 0) || dwRead == 0) {
                break;
            }
            out += tmp;
        }
        CloseHandle(procInfo.hProcess);
        CloseHandle(procInfo.hThread);
        return true;
    }

    bool Win32Platform::ShowOpenFileDialog(void *owner, std::string &outPath, const std::string &title,
                                           const std::string &filter)
    {
        wchar_t fileName[MAX_PATH] = {0};
        const std::wstring wideTitle = UTF8ToWide(title);
        const std::wstring wideFilter = BuildDialogFilter(filter);

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = static_cast<HWND>(owner);
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = wideTitle.empty() ? nullptr : wideTitle.c_str();
        ofn.lpstrFilter = wideFilter.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

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

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = static_cast<HWND>(owner);
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = wideTitle.empty() ? nullptr : wideTitle.c_str();
        ofn.lpstrFilter = wideFilter.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_EXPLORER;

        if (::GetSaveFileNameW(&ofn) == TRUE) {
            outPath = WideToUTF8(fileName);
            return true;
        }
        return false;
    }
}