//
// Created by blues on 2026/10/3.
//

#include <framework/platform/Process.h>
#include <core/platform/Platform.h>

namespace sky {

#if SKY_PLATFORM_WINDOWS
    ProcessPtr CreateWin32Process();
#elif SKY_PLATFORM_MACOS || SKY_PLATFORM_LINUX
    ProcessPtr CreatePosixProcess();
#endif

    namespace {

        // Stub returned where no process backend is compiled (mobile, unknown platforms).
        class UnsupportedProcess : public IProcess {
        public:
            bool Start(const ProcessDesc &) override
            {
                status = ProcessStatus::Unsupported;
                return false;
            }

            ProcessStatus GetStatus() const override { return status; }
            bool WriteStdin(const uint8_t *, size_t) override { return false; }

            size_t ReadStdout(uint8_t *, size_t, bool &eof) override
            {
                eof = true;
                return 0;
            }

            size_t ReadStderr(uint8_t *, size_t, bool &eof) override
            {
                eof = true;
                return 0;
            }

            bool WaitFor(uint32_t, int &exitCode) override
            {
                exitCode = -1;
                return false;
            }

            void Kill() override {}
            bool IsRunning() const override { return false; }

        private:
            ProcessStatus status = ProcessStatus::Unsupported;
        };

    } // namespace

    ProcessPtr CreateProcess()
    {
#if SKY_PLATFORM_WINDOWS
        return CreateWin32Process();
#elif SKY_PLATFORM_MACOS || SKY_PLATFORM_LINUX
        return CreatePosixProcess();
#else
        return std::make_unique<UnsupportedProcess>();
#endif
    }

} // namespace sky
