//
// Created on 2026/10/05.
//

#include <framework/project/ProjectLock.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

static const char *TAG = "ProjectLock";

namespace sky {

    namespace {
        uint64_t CurrentPid()
        {
#if defined(_WIN32)
            return static_cast<uint64_t>(::GetCurrentProcessId());
#else
            return static_cast<uint64_t>(::getpid());
#endif
        }

        bool IsProcessAlive(uint64_t pid)
        {
            if (pid == 0) {
                return false;
            }
#if defined(_WIN32)
            HANDLE handle = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
            if (handle == nullptr) {
                return false;
            }
            DWORD exitCode = 0;
            const bool alive = ::GetExitCodeProcess(handle, &exitCode) != 0 && exitCode == STILL_ACTIVE;
            ::CloseHandle(handle);
            return alive;
#else
            return ::kill(static_cast<pid_t>(pid), 0) == 0;
#endif
        }

        uint64_t ReadPid(const std::string &path)
        {
            std::ifstream in(path);
            uint64_t pid = 0;
            if (in) {
                in >> pid;
            }
            return pid;
        }
    } // namespace

    ProjectLock::~ProjectLock()
    {
        Release();
    }

    bool ProjectLock::Acquire(const std::string &projectDir)
    {
        if (locked) {
            return true;
        }
        const std::filesystem::path cacheDir = std::filesystem::path(projectDir) / "cache";
        std::error_code error;
        std::filesystem::create_directories(cacheDir, error);
        lockPath = (cacheDir / "editor.lock").string();

        const uint64_t existing = ReadPid(lockPath);
        if (existing != 0 && existing != CurrentPid() && IsProcessAlive(existing)) {
            return false;
        }

        std::ofstream out(lockPath, std::ios::trunc);
        if (!out) {
            return false;
        }
        out << CurrentPid();
        locked = true;
        return true;
    }

    void ProjectLock::Release()
    {
        if (!locked) {
            return;
        }
        // Only remove the lock if we still own it.
        if (ReadPid(lockPath) == CurrentPid()) {
            std::error_code error;
            std::filesystem::remove(lockPath, error);
        }
        locked = false;
    }

} // namespace sky
