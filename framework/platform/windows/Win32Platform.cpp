//
// Created by Zach Lee on 2022/9/25.
//

#include "Win32Platform.h"

#include <xstring>
#include <filesystem>

#include <windows.h>
#include <shlobj_core.h>

static const char* TAG = "Win32Platform";

namespace sky {

    std::string StringWideCharToUtf8(const std::wstring_view &strWideChar)
    {
        std::string ret;
        if (!strWideChar.empty()) {
            int num = WideCharToMultiByte(CP_UTF8, 0, strWideChar.data(), -1, nullptr, 0, nullptr, FALSE);
            if (num != 0) {
                ret.resize(num + 1, 0);
                num = WideCharToMultiByte(CP_UTF8, 0, strWideChar.data(), -1, ret.data(), num + 1, nullptr, FALSE);
            }
        }
        return ret;
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

        return std::filesystem::path(StringWideCharToUtf8(fullPath)).parent_path().string();
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
        if (!success) {
            return false;
        }

        WaitForSingleObject(procInfo.hProcess, 5000);
        for (;;) {
            DWORD dwRead = 0;
            DWORD dwAvail = 0;

            if (!::PeekNamedPipe(pipe->read, nullptr, 0, nullptr, &dwAvail, nullptr)) {
                break;
            }
            if (dwAvail == 0) {
                break;
            }
            std::string tmp(dwAvail, 0);
            if (!::ReadFile(pipe->read, (char*)tmp.data(), dwAvail, &dwRead, nullptr) || dwRead == 0) {
                break;
            }
            out += tmp;
        }
        CloseHandle(procInfo.hProcess);
        CloseHandle(procInfo.hThread);
        return true;
    }
}