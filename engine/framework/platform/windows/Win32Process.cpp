//
// Created by blues on 2026/10/3.
//

#include "Win32Process.h"

#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>

namespace sky {

    namespace {

        constexpr uint32_t POLL_TIMEOUT_MS = 20;
        constexpr uint32_t POLL_STEP_MS    = 5;

        std::wstring Utf8ToWide(const std::string &text)
        {
            if (text.empty()) {
                return {};
            }
            const int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
            if (len <= 0) {
                return {};
            }
            std::wstring out(static_cast<size_t>(len), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), len);
            return out;
        }

        void AppendQuotedArg(std::wstring &out, const std::wstring &arg)
        {
            if (!out.empty()) {
                out.push_back(L' ');
            }
            if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
                out += arg;
                return;
            }

            out.push_back(L'"');
            size_t backslashes = 0;
            for (size_t i = 0; i < arg.size(); ++i) {
                const wchar_t c = arg[i];
                if (c == L'\\') {
                    ++backslashes;
                    continue;
                }
                if (c == L'"') {
                    out.append(backslashes * 2 + 1, L'\\');
                    out.push_back(L'"');
                } else {
                    out.append(backslashes, L'\\');
                    out.push_back(c);
                }
                backslashes = 0;
            }
            out.append(backslashes * 2, L'\\');
            out.push_back(L'"');
        }

        std::wstring BuildCommandLine(const std::vector<std::string> &args)
        {
            std::wstring line;
            for (const auto &arg : args) {
                AppendQuotedArg(line, Utf8ToWide(arg));
            }
            return line;
        }

        // UTF-16 environment block: "KEY=VALUE\0" ... "\0"; empty means inherit.
        std::vector<wchar_t> BuildEnvironment(const std::vector<std::string> &env)
        {
            std::vector<wchar_t> block;
            if (env.empty()) {
                return block;
            }
            for (const auto &entry : env) {
                const std::wstring wide = Utf8ToWide(entry);
                block.insert(block.end(), wide.begin(), wide.end());
                block.push_back(L'\0');
            }
            block.push_back(L'\0');
            return block;
        }

    } // namespace

    class Win32Process : public IProcess {
    public:
        Win32Process() = default;
        ~Win32Process() override
        {
            // Do not rely solely on the job object: AssignProcessToJobObject can fail when
            // the engine already belongs to a job.
            if (process != nullptr && IsRunning()) {
                TerminateProcess(process, 1);
            }
            CloseStdinWrite();
            CloseStdout();
            CloseStderr();
            if (process != nullptr) {
                CloseHandle(process);
                process = nullptr;
            }
            if (job != nullptr) {
                CloseHandle(job);
                job = nullptr;
            }
        }

        bool Start(const ProcessDesc &desc) override
        {
            if (desc.args.empty()) {
                status = ProcessStatus::NotFound;
                return false;
            }

            SECURITY_ATTRIBUTES sa = {};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;
            sa.lpSecurityDescriptor = nullptr;

            HANDLE childStdinRead = nullptr;
            HANDLE childStdoutWrite = nullptr;
            HANDLE childStderrWrite = nullptr;

            if (!CreatePipe(&childStdinRead, &stdinWrite, &sa, 0) ||
                !CreatePipe(&stdoutRead, &childStdoutWrite, &sa, 0) ||
                !CreatePipe(&stderrRead, &childStderrWrite, &sa, 0)) {
                CloseStdinWrite();
                CloseStdout();
                CloseStderr();
                CloseHandle(childStdinRead);
                CloseHandle(childStdoutWrite);
                CloseHandle(childStderrWrite);
                status = ProcessStatus::LaunchFailed;
                return false;
            }

            SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOW startup = {};
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdInput = childStdinRead;
            startup.hStdOutput = childStdoutWrite;
            startup.hStdError = childStderrWrite;

            PROCESS_INFORMATION info = {};

            std::wstring commandLine = BuildCommandLine(desc.args);
            std::vector<wchar_t> envBlock = BuildEnvironment(desc.env);
            std::wstring cwd = Utf8ToWide(desc.cwd);

            const BOOL ok = CreateProcessW(
                nullptr, commandLine.data(),
                nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                envBlock.empty() ? nullptr : envBlock.data(),
                cwd.empty() ? nullptr : cwd.c_str(),
                &startup, &info);

            CloseHandle(childStdinRead);
            CloseHandle(childStdoutWrite);
            CloseHandle(childStderrWrite);

            if (!ok) {
                const DWORD error = GetLastError();
                CloseStdinWrite();
                CloseStdout();
                CloseStderr();
                status = (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
                             ? ProcessStatus::NotFound
                             : ProcessStatus::LaunchFailed;
                return false;
            }

            process = info.hProcess;
            CloseHandle(info.hThread);

            job = CreateJobObjectW(nullptr, nullptr);
            if (job != nullptr) {
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
                limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
                AssignProcessToJobObject(job, process);
            }

            status = ProcessStatus::Running;
            return true;
        }

        ProcessStatus GetStatus() const override
        {
            if (process != nullptr && status == ProcessStatus::Running) {
                DWORD code = 0;
                if (GetExitCodeProcess(process, &code) && code != STILL_ACTIVE) {
                    return ProcessStatus::Exited;
                }
            }
            return status;
        }

        bool WriteStdin(const uint8_t *data, size_t size) override
        {
            if (stdinWrite == nullptr) {
                return false;
            }
            size_t written = 0;
            while (written < size) {
                const DWORD chunk = static_cast<DWORD>(std::min<size_t>(size - written, 1u << 20));
                DWORD done = 0;
                if (!WriteFile(stdinWrite, data + written, chunk, &done, nullptr) || done == 0) {
                    return false;
                }
                written += done;
            }
            return true;
        }

        size_t ReadStdout(uint8_t *dst, size_t cap, bool &eof) override
        {
            return ReadAvailable(stdoutRead, dst, cap, eof);
        }

        size_t ReadStderr(uint8_t *dst, size_t cap, bool &eof) override
        {
            return ReadAvailable(stderrRead, dst, cap, eof);
        }

        bool WaitFor(uint32_t timeoutMs, int &exitCode) override
        {
            if (process == nullptr) {
                return false;
            }
            const DWORD wait = WaitForSingleObject(process, timeoutMs);
            if (wait == WAIT_TIMEOUT) {
                return false;
            }
            DWORD code = 0;
            GetExitCodeProcess(process, &code);
            exitCode = static_cast<int>(code);
            status = ProcessStatus::Exited;
            return true;
        }

        void Kill() override
        {
            if (process != nullptr) {
                TerminateProcess(process, 1);
                status = ProcessStatus::Exited;
            }
        }

        bool IsRunning() const override
        {
            return GetStatus() == ProcessStatus::Running;
        }

    private:
        size_t ReadAvailable(HANDLE handle, uint8_t *dst, size_t cap, bool &eof)
        {
            eof = false;
            if (handle == nullptr) {
                eof = true;
                return 0;
            }

            DWORD available = 0;
            uint32_t waited = 0;
            for (;;) {
                if (PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr)) {
                    if (available > 0) {
                        break;
                    }
                } else {
                    const DWORD err = GetLastError();
                    eof = (err == ERROR_BROKEN_PIPE || err == ERROR_HANDLE_EOF);
                    return 0;
                }

                if (waited >= POLL_TIMEOUT_MS || !IsRunning()) {
                    return 0;
                }
                Sleep(POLL_STEP_MS);
                waited += POLL_STEP_MS;
            }

            const DWORD toRead = static_cast<DWORD>(std::min<size_t>(cap, available));
            DWORD read = 0;
            if (!ReadFile(handle, dst, toRead, &read, nullptr)) {
                const DWORD err = GetLastError();
                eof = (err == ERROR_BROKEN_PIPE || err == ERROR_HANDLE_EOF);
                return 0;
            }
            if (read == 0) {
                eof = true;
            }
            return read;
        }

        void CloseStdinWrite()
        {
            if (stdinWrite != nullptr) {
                CloseHandle(stdinWrite);
                stdinWrite = nullptr;
            }
        }

        void CloseStdout()
        {
            if (stdoutRead != nullptr) {
                CloseHandle(stdoutRead);
                stdoutRead = nullptr;
            }
        }

        void CloseStderr()
        {
            if (stderrRead != nullptr) {
                CloseHandle(stderrRead);
                stderrRead = nullptr;
            }
        }

        HANDLE process = nullptr;
        HANDLE job = nullptr;
        HANDLE stdinWrite = nullptr;
        HANDLE stdoutRead = nullptr;
        HANDLE stderrRead = nullptr;
        ProcessStatus status = ProcessStatus::None;
    };

    ProcessPtr CreateWin32Process()
    {
        return std::make_unique<Win32Process>();
    }

} // namespace sky
