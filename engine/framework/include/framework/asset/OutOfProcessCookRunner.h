//
// Created by blues on 2026/10/3.
//

#pragma once

#include <framework/asset/ICookRunner.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace sky {

    class IProcess;
    class FrameChannel;

    struct CookRunnerConfig {
        std::string workerPath;
        // Extra arguments placed immediately after the executable (e.g. a worker mode).
        std::vector<std::string> extraArgs;
        std::string projectPath;      // passed to the worker as --project
        std::string enginePath;       // passed to the worker as --engine
        std::string intermediatePath; // optional, passed as --intermediate
        std::string platform;         // platform target, e.g. "Win32"
        uint32_t    timeoutMs = 10u * 60u * 1000u;
    };

    // Dispatches cooks to a persistent worker process and raises the same
    // IAssetEvent::OnAssetBuildFinished completion event as the in-process runner (D5/D7).
    class OutOfProcessCookRunner : public ICookRunner {
    public:
        explicit OutOfProcessCookRunner(CookRunnerConfig config);
        ~OutOfProcessCookRunner() override;

        OutOfProcessCookRunner(const OutOfProcessCookRunner &) = delete;
        OutOfProcessCookRunner &operator=(const OutOfProcessCookRunner &) = delete;

        void SetCompletion(CookCompletion handler) override;
        bool Request(const CookJob &job) override;
        void Drain() override;

    private:
        struct InFlight {
            CookJob  job;
            uint64_t deadlineMs = 0;
        };

        bool StartWorker();
        void StopWorker();
        void MonitorLoop();

        void HandleFrame(const uint8_t *data, size_t size);
        void OnResult(uint64_t id, const Uuid &uuid, const std::string &target, int32_t retCode, const std::string &error);
        void NotifyCompletion(const Uuid &uuid, const std::string &target, AssetBuildRetCode code, const std::string &error);
        void FailAllInFlight(const char *reason);
        void OnChannelFailure(const char *reason);

        uint64_t NowMs() const;

        CookRunnerConfig              config;
        std::unique_ptr<IProcess>     process;
        std::unique_ptr<FrameChannel> channel;
        std::thread                   monitorThread;

        CookCompletion                completion;
        std::mutex                    mutex;
        // Serializes Request/Drain/worker start-stop so concurrent loaders cannot race
        // worker lifecycle (never held by the monitor thread).
        std::mutex                    lifecycleMutex;
        std::condition_variable       cv;
        bool                          workerReady = false;
        bool                          workerFailed = false;
        bool                          stopping = false;
        bool                          monitorStop = false;
        uint64_t                      nextId = 1;
        std::unordered_map<uint64_t, InFlight> inFlight;
    };

} // namespace sky
