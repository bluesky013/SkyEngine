//
// Created by blues on 2026/10/3.
//

// Shared POSIX backend for macOS and Linux. Uses only the portable subset:
// posix_spawn / posix_spawn_file_actions_* / posix_spawnattr_* / pipe + fcntl(CLOEXEC)
// / poll / waitpid / kill. No pipe2, no parent-death signal.

#include "PosixProcess.h"

#include <cerrno>
#include <csignal>
#include <mutex>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

// posix_spawn_file_actions_addchdir_np availability:
//  - macOS: declared in <spawn.h> (10.15+), no _GNU_SOURCE needed
//  - glibc: declared only with _GNU_SOURCE and >= 2.29
#if defined(__APPLE__)
    #define SKY_POSIX_HAVE_ADDCHDIR_NP 1
#elif defined(__GLIBC__) && defined(_GNU_SOURCE) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 29))
    #define SKY_POSIX_HAVE_ADDCHDIR_NP 1
#else
    #define SKY_POSIX_HAVE_ADDCHDIR_NP 0
#endif

namespace sky {

    namespace {

        constexpr uint32_t POLL_TIMEOUT_MS = 20;
        constexpr uint32_t POLL_STEP_MS    = 5;

        void EnsureSigPipeIgnored()
        {
            static std::once_flag once;
            std::call_once(once, []() { std::signal(SIGPIPE, SIG_IGN); });
        }

        void SleepMs(uint32_t ms)
        {
            struct timespec ts = {};
            ts.tv_sec = ms / 1000;
            ts.tv_nsec = static_cast<long>(ms % 1000) * 1000000L;
            nanosleep(&ts, nullptr);
        }

        // Both pipe ends are close-on-exec; the child's dup2 target clears the flag, and
        // the parent ends must not leak into the child.
        bool MakePipe(int fds[2])
        {
            if (pipe(fds) != 0) {
                return false;
            }
            for (int i = 0; i < 2; ++i) {
                const int flags = fcntl(fds[i], F_GETFD);
                if (flags >= 0) {
                    fcntl(fds[i], F_SETFD, flags | FD_CLOEXEC);
                }
            }
            return true;
        }

        std::vector<char *> BuildArgv(const std::vector<std::string> &args)
        {
            std::vector<char *> argv;
            argv.reserve(args.size() + 1);
            for (const auto &arg : args) {
                argv.push_back(const_cast<char *>(arg.c_str()));
            }
            argv.push_back(nullptr);
            return argv;
        }

        std::vector<char *> BuildEnvp(const std::vector<std::string> &env)
        {
            std::vector<char *> envp;
            if (env.empty()) {
                return envp;
            }
            envp.reserve(env.size() + 1);
            for (const auto &entry : env) {
                envp.push_back(const_cast<char *>(entry.c_str()));
            }
            envp.push_back(nullptr);
            return envp;
        }

        int SpawnPosix(const std::string &cwd, std::vector<char *> &argv, char **env,
                       int inPipe[2], int outPipe[2], int errPipe[2], pid_t &outPid)
        {
            posix_spawn_file_actions_t actions;
            posix_spawn_file_actions_init(&actions);

            posix_spawn_file_actions_adddup2(&actions, inPipe[0], STDIN_FILENO);
            posix_spawn_file_actions_adddup2(&actions, outPipe[1], STDOUT_FILENO);
            posix_spawn_file_actions_adddup2(&actions, errPipe[1], STDERR_FILENO);

            posix_spawn_file_actions_addclose(&actions, inPipe[0]);
            posix_spawn_file_actions_addclose(&actions, outPipe[1]);
            posix_spawn_file_actions_addclose(&actions, errPipe[1]);
            posix_spawn_file_actions_addclose(&actions, inPipe[1]);
            posix_spawn_file_actions_addclose(&actions, outPipe[0]);
            posix_spawn_file_actions_addclose(&actions, errPipe[0]);

#if SKY_POSIX_HAVE_ADDCHDIR_NP
            if (!cwd.empty()) {
                posix_spawn_file_actions_addchdir_np(&actions, cwd.c_str());
            }
#endif

            posix_spawnattr_t attr;
            posix_spawnattr_init(&attr);
            posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
            posix_spawnattr_setpgroup(&attr, 0);

            pid_t pid = -1;
            const int rc = posix_spawnp(&pid, argv[0], &actions, &attr, argv.data(), env);

            posix_spawn_file_actions_destroy(&actions);
            posix_spawnattr_destroy(&attr);

            if (rc == 0) {
                outPid = pid;
            }
            return rc;
        }

        // Fallback used only when addchdir_np is unavailable and a cwd must be applied.
        // Between fork and exec only async-signal-safe calls run; all buffers are pre-built.
        int SpawnForkExec(const std::string &cwd, std::vector<char *> &argv, char **env,
                          int inPipe[2], int outPipe[2], int errPipe[2], pid_t &outPid)
        {
            const char *cwdStr = cwd.c_str();
            const int   inRead = inPipe[0];
            const int   outWrite = outPipe[1];
            const int   errWrite = errPipe[1];
            const int   parentEnds[3] = {inPipe[1], outPipe[0], errPipe[0]};

            const pid_t pid = fork();
            if (pid < 0) {
                return errno;
            }
            if (pid == 0) {
                setpgid(0, 0);
                if (cwdStr != nullptr && cwdStr[0] != '\0') {
                    if (chdir(cwdStr) != 0) {
                        _exit(127);
                    }
                }
                dup2(inRead, STDIN_FILENO);
                dup2(outWrite, STDOUT_FILENO);
                dup2(errWrite, STDERR_FILENO);

                if (inRead > 2) {
                    close(inRead);
                }
                if (outWrite > 2) {
                    close(outWrite);
                }
                if (errWrite > 2) {
                    close(errWrite);
                }
                for (int i = 0; i < 3; ++i) {
                    if (parentEnds[i] > 2) {
                        close(parentEnds[i]);
                    }
                }

                if (argv[0][0] == '/') {
                    execve(argv[0], argv.data(), env);
                } else {
                    // execvp searches PATH but does not take a custom environment.
                    execvp(argv[0], argv.data());
                }
                _exit(127);
            }

            outPid = pid;
            return 0;
        }

        size_t ReadAvailable(int fd, uint8_t *dst, size_t cap, bool &eof)
        {
            eof = false;
            if (fd < 0 || cap == 0) {
                eof = true;
                return 0;
            }

            struct pollfd pfd = {};
            pfd.fd = fd;
            pfd.events = POLLIN;

            const int rc = poll(&pfd, 1, static_cast<int>(POLL_TIMEOUT_MS));
            if (rc == 0) {
                return 0;
            }
            if (rc < 0) {
                if (errno == EINTR) {
                    return 0;
                }
                eof = true;
                return 0;
            }
            if ((pfd.revents & (POLLERR | POLLNVAL)) != 0) {
                eof = true;
                return 0;
            }

            const ssize_t n = read(fd, dst, cap);
            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                    return 0;
                }
                eof = true;
                return 0;
            }
            if (n == 0) {
                eof = true;
                return 0;
            }
            return static_cast<size_t>(n);
        }

    } // namespace

    class PosixProcess : public IProcess {
    public:
        PosixProcess() = default;

        ~PosixProcess() override
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (pid > 0 && !reaped) {
                kill(-pid, SIGKILL);
                int status = 0;
                waitpid(pid, &status, 0);
                reaped = true;
            }
            CloseFd(stdinWrite);
            CloseFd(stdoutRead);
            CloseFd(stderrRead);
        }

        bool Start(const ProcessDesc &desc) override
        {
            if (desc.args.empty()) {
                state = ProcessStatus::NotFound;
                return false;
            }
            EnsureSigPipeIgnored();

            int inPipe[2] = {-1, -1};
            int outPipe[2] = {-1, -1};
            int errPipe[2] = {-1, -1};

            if (!MakePipe(inPipe)) {
                state = ProcessStatus::LaunchFailed;
                return false;
            }
            if (!MakePipe(outPipe)) {
                CloseFd(inPipe[0]);
                CloseFd(inPipe[1]);
                state = ProcessStatus::LaunchFailed;
                return false;
            }
            if (!MakePipe(errPipe)) {
                CloseFd(inPipe[0]);
                CloseFd(inPipe[1]);
                CloseFd(outPipe[0]);
                CloseFd(outPipe[1]);
                state = ProcessStatus::LaunchFailed;
                return false;
            }

            std::vector<char *> argv = BuildArgv(desc.args);
            std::vector<char *> envp = BuildEnvp(desc.env);
            char **env = desc.env.empty() ? environ : envp.data();

            pid_t child = -1;
            int rc = 0;
#if SKY_POSIX_HAVE_ADDCHDIR_NP
            rc = SpawnPosix(desc.cwd, argv, env, inPipe, outPipe, errPipe, child);
#else
            if (desc.cwd.empty()) {
                rc = SpawnPosix(desc.cwd, argv, env, inPipe, outPipe, errPipe, child);
            } else {
                rc = SpawnForkExec(desc.cwd, argv, env, inPipe, outPipe, errPipe, child);
            }
#endif

            // The parent never keeps the child ends.
            CloseFd(inPipe[0]);
            CloseFd(outPipe[1]);
            CloseFd(errPipe[1]);

            if (rc != 0) {
                CloseFd(inPipe[1]);
                CloseFd(outPipe[0]);
                CloseFd(errPipe[0]);
                state = (rc == ENOENT) ? ProcessStatus::NotFound : ProcessStatus::LaunchFailed;
                return false;
            }

            std::lock_guard<std::mutex> lock(mutex);
            pid = child;
            stdinWrite = inPipe[1];
            stdoutRead = outPipe[0];
            stderrRead = errPipe[0];
            state = ProcessStatus::Running;
            return true;
        }

        ProcessStatus GetStatus() const override
        {
            std::lock_guard<std::mutex> lock(mutex);
            PollExitLocked();
            return state;
        }

        bool WriteStdin(const uint8_t *data, size_t size) override
        {
            size_t written = 0;
            while (written < size) {
                const ssize_t n = write(stdinWrite, data + written, size - written);
                if (n < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return false;
                }
                written += static_cast<size_t>(n);
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
            uint32_t waited = 0;
            for (;;) {
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    PollExitLocked();
                    if (pid <= 0 || reaped) {
                        exitCode = exitStatus;
                        return reaped;
                    }
                    if (waited >= timeoutMs) {
                        return false;
                    }
                }
                SleepMs(POLL_STEP_MS);
                waited += POLL_STEP_MS;
            }
        }

        void Kill() override
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (pid <= 0 || reaped) {
                return;
            }
            kill(-pid, SIGKILL);
            int status = 0;
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
            reaped = true;
            state = ProcessStatus::Exited;
        }

        bool IsRunning() const override
        {
            std::lock_guard<std::mutex> lock(mutex);
            PollExitLocked();
            return state == ProcessStatus::Running && !reaped;
        }

    private:
        // Reaps the child if it has exited, so a crashed worker becomes visible without a
        // blocking WaitFor (the runner polls IsRunning in its monitor loop).
        void PollExitLocked() const
        {
            if (pid <= 0 || reaped) {
                return;
            }
            int status = 0;
            const pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid) {
                exitStatus = WIFEXITED(status) ? WEXITSTATUS(status)
                                               : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1);
                reaped = true;
                state = ProcessStatus::Exited;
            }
        }

        static void CloseFd(int &fd)
        {
            if (fd >= 0) {
                close(fd);
                fd = -1;
            }
        }

        mutable std::mutex mutex;
        mutable pid_t        pid = -1;
        mutable int          stdinWrite = -1;
        mutable int          stdoutRead = -1;
        mutable int          stderrRead = -1;
        mutable int          exitStatus = -1;
        mutable bool         reaped = false;
        mutable ProcessStatus state = ProcessStatus::None;
    };

    ProcessPtr CreatePosixProcess()
    {
        return std::make_unique<PosixProcess>();
    }

} // namespace sky
