//
// Created by blues on 2026/10/3.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sky {

    struct ProcessDesc {
        std::vector<std::string> args; // argv; args[0] is the executable
        std::vector<std::string> env;  // "KEY=VALUE"; empty inherits the parent environment
        std::string              cwd;  // empty inherits the parent working directory
    };

    enum class ProcessStatus : uint32_t {
        None = 0,
        Running,
        Exited,
        Unsupported,
        NotFound,
        LaunchFailed,
    };

    // Child process with piped stdio. ReadStdout/ReadStderr block for at most a short
    // internal poll interval, so a reader loop can observe shutdown without a cancel API.
    // Returns the number of bytes read; 0 with eof=true means end of stream.
    class IProcess {
    public:
        virtual ~IProcess() = default;

        virtual bool Start(const ProcessDesc &desc) = 0;
        virtual ProcessStatus GetStatus() const = 0;

        virtual bool WriteStdin(const uint8_t *data, size_t size) = 0;
        virtual size_t ReadStdout(uint8_t *dst, size_t cap, bool &eof) = 0;
        virtual size_t ReadStderr(uint8_t *dst, size_t cap, bool &eof) = 0;

        // Returns true if the child exited within the timeout (exitCode is set),
        // false on timeout.
        virtual bool WaitFor(uint32_t timeoutMs, int &exitCode) = 0;

        // Idempotent, safe from any thread.
        virtual void Kill() = 0;
        virtual bool IsRunning() const = 0;
    };

    using ProcessPtr = std::unique_ptr<IProcess>;

    // Factory: returns a platform backend, or an "unsupported" stub where no backend exists.
    ProcessPtr CreateProcess();

} // namespace sky
