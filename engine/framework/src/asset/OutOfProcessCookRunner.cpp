//
// Created by blues on 2026/10/3.
//

#include <framework/asset/OutOfProcessCookRunner.h>

#include <framework/asset/AssetCommon.h>
#include <framework/asset/AssetEvent.h>
#include <framework/asset/CookProtocol.h>
#include <framework/ipc/FrameChannel.h>
#include <framework/platform/Process.h>
#include <core/logger/Logger.h>

#include <chrono>
#include <utility>
#include <vector>

namespace sky {

    namespace {

        constexpr const char *TAG = "CookRunner";
        constexpr uint32_t    HANDSHAKE_TIMEOUT_MS = 5000;
        constexpr uint32_t    MONITOR_INTERVAL_MS  = 200;
        constexpr uint32_t    SHUTDOWN_WAIT_MS     = 2000;

    } // namespace

    void OutOfProcessCookRunner::NotifyCompletion(const Uuid &uuid, const std::string &target, AssetBuildRetCode code, const std::string &error)
    {
        AssetBuildResult result = {};
        result.uuid = uuid;
        result.target = target;
        result.retCode = code;
        result.error = error;

        AsseEvent::BroadCast(uuid, &IAssetEvent::OnAssetBuildFinished, result);

        CookCompletion handler;
        {
            std::lock_guard<std::mutex> lock(mutex);
            handler = completion;
        }
        if (handler) {
            handler(result);
        }
    }

    OutOfProcessCookRunner::OutOfProcessCookRunner(CookRunnerConfig inConfig)
        : config(std::move(inConfig))
    {
    }

    OutOfProcessCookRunner::~OutOfProcessCookRunner()
    {
        Drain();
    }

    void OutOfProcessCookRunner::SetCompletion(CookCompletion handler)
    {
        std::lock_guard<std::mutex> lock(mutex);
        completion = std::move(handler);
    }

    uint64_t OutOfProcessCookRunner::NowMs() const
    {
        using namespace std::chrono;
        return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }

    bool OutOfProcessCookRunner::StartWorker()
    {
        process = CreateProcess();
        if (!process) {
            return false;
        }

        ProcessDesc desc = {};
        desc.args.push_back(config.workerPath);
        for (const auto &arg : config.extraArgs) {
            desc.args.push_back(arg);
        }
        if (!config.projectPath.empty()) {
            desc.args.push_back("--project");
            desc.args.push_back(config.projectPath);
        }
        if (!config.enginePath.empty()) {
            desc.args.push_back("--engine");
            desc.args.push_back(config.enginePath);
        }
        if (!config.intermediatePath.empty()) {
            desc.args.push_back("--intermediate");
            desc.args.push_back(config.intermediatePath);
        }
        if (!config.platform.empty()) {
            desc.args.push_back("--platform");
            desc.args.push_back(config.platform);
        }

        if (!process->Start(desc)) {
            process.reset();
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            workerReady = false;
            workerFailed = false;
        }

        channel = std::make_unique<FrameChannel>(
            [this](uint8_t *dst, size_t cap, bool &eof) { return process->ReadStdout(dst, cap, eof); },
            [this](const uint8_t *data, size_t size) { return process->WriteStdin(data, size); },
            [this](uint8_t *dst, size_t cap, bool &eof) { return process->ReadStderr(dst, cap, eof); });
        channel->SetFrameHandler([this](const uint8_t *data, size_t size) { HandleFrame(data, size); });
        channel->SetLogHandler([](const std::string &line) { LOG_I(TAG, "%s", line.c_str()); });
        channel->SetErrorHandler([this](const char *reason) { OnChannelFailure(reason); });
        channel->Start();

        CookMessage hello = {};
        hello.type = CookMessageType::Hello;
        hello.protocol = COOK_PROTOCOL_VERSION;
        channel->WriteFrame(EncodeCookMessage(hello));

        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::milliseconds(HANDSHAKE_TIMEOUT_MS), [this]() {
            return workerReady || workerFailed;
        });
        const bool ok = workerReady && !workerFailed;
        if (!ok) {
            LOG_E(TAG, "worker handshake failed");
        }

        monitorStop = false;
        monitorThread = std::thread([this]() { MonitorLoop(); });
        return ok;
    }

    void OutOfProcessCookRunner::StopWorker()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!process && !channel) {
                return;
            }
            monitorStop = true; // signal the monitor loop to exit before we join it
        }

        if (channel) {
            CookMessage shutdown = {};
            shutdown.type = CookMessageType::Shutdown;
            channel->WriteFrame(EncodeCookMessage(shutdown));
            channel->Stop();
        }
        if (process) {
            int exitCode = 0;
            if (!process->WaitFor(SHUTDOWN_WAIT_MS, exitCode)) {
                process->Kill();
            }
        }
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
        channel.reset();
        process.reset();
        {
            std::lock_guard<std::mutex> lock(mutex);
            workerReady = false;
        }
    }

    bool OutOfProcessCookRunner::Request(const CookJob &job)
    {
        // NotifyCompletion is invoked after releasing lifecycleMutex: a completion handler
        // must be free to call back into Request/Drain without deadlocking.
        const char *failure = nullptr;
        {
            std::lock_guard<std::mutex> lifecycle(lifecycleMutex);

            bool ready = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (stopping) {
                    return false;
                }
                ready = workerReady && !workerFailed && process && process->IsRunning();
            }

            if (!ready) {
                StopWorker();
                if (!StartWorker()) {
                    failure = "worker start/handshake failed";
                }
            }

            if (failure == nullptr) {
                uint64_t id = 0;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    id = nextId++;
                    inFlight[id] = InFlight{job, NowMs() + config.timeoutMs};
                }

                CookMessage message = {};
                message.type = CookMessageType::Cook;
                message.id = id;
                message.uuid = job.uuid;
                message.target = job.target;
                message.path = job.path;

                if (!channel->WriteFrame(EncodeCookMessage(message))) {
                    std::lock_guard<std::mutex> lock(mutex);
                    inFlight.erase(id);
                    failure = "failed to write cook request";
                }
            }
        }

        if (failure != nullptr) {
            NotifyCompletion(job.uuid, job.target, AssetBuildRetCode::FAILED, failure);
            return false;
        }
        return true;
    }

    void OutOfProcessCookRunner::Drain()
    {
        {
            std::lock_guard<std::mutex> lifecycle(lifecycleMutex);
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (stopping) {
                    return;
                }
                stopping = true;
            }
            if (monitorThread.joinable()) {
                monitorThread.join();
            }
            StopWorker();
        }

        FailAllInFlight("runner drained");
    }

    void OutOfProcessCookRunner::MonitorLoop()
    {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(MONITOR_INTERVAL_MS));

            std::vector<CookJob> failed;
            bool timedOut = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (stopping || monitorStop) {
                    return;
                }
                if (!process || !process->IsRunning()) {
                    for (auto &[id, flight] : inFlight) {
                        failed.push_back(flight.job);
                    }
                    inFlight.clear();
                    workerReady = false;
                    workerFailed = true;
                } else {
                    const uint64_t now = NowMs();
                    for (auto it = inFlight.begin(); it != inFlight.end();) {
                        if (it->second.deadlineMs <= now) {
                            failed.push_back(it->second.job);
                            it = inFlight.erase(it);
                        } else {
                            ++it;
                        }
                    }
                    timedOut = !failed.empty();
                }
            }

            // Never call StopWorker() from the monitor thread (it would join itself);
            // the worker is torn down lazily by Request()/Drain().
            if (!failed.empty()) {
                const char *reason = timedOut ? "cook timed out" : "worker exited";
                for (const auto &job : failed) {
                    NotifyCompletion(job.uuid, job.target, AssetBuildRetCode::FAILED, reason);
                }
                if (timedOut && process) {
                    process->Kill();
                }
            }
        }
    }

    void OutOfProcessCookRunner::HandleFrame(const uint8_t *data, size_t size)
    {
        CookMessage message;
        if (!DecodeCookMessage(std::string(reinterpret_cast<const char *>(data), size), message)) {
            LOG_W(TAG, "dropping malformed frame");
            return;
        }

        switch (message.type) {
            case CookMessageType::Ready: {
                std::lock_guard<std::mutex> lock(mutex);
                if (message.protocol != COOK_PROTOCOL_VERSION) {
                    workerFailed = true;
                } else {
                    workerReady = true;
                }
                cv.notify_all();
                break;
            }
            case CookMessageType::Result:
                OnResult(message.id, message.uuid, message.target, message.retCode, message.error);
                break;
            case CookMessageType::Pong:
            default:
                break;
        }
    }

    void OutOfProcessCookRunner::OnResult(uint64_t id, const Uuid &uuid, const std::string &target, int32_t retCode, const std::string &error)
    {
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            const auto it = inFlight.find(id);
            if (it != inFlight.end()) {
                inFlight.erase(it);
                found = true;
            }
        }

        if (!found) {
            LOG_W(TAG, "result for unknown request id %llu", static_cast<unsigned long long>(id));
            return;
        }

        const AssetBuildRetCode code = (retCode == 0) ? AssetBuildRetCode::SUCCESS : AssetBuildRetCode::FAILED;
        NotifyCompletion(uuid, target, code, error);
    }

    void OutOfProcessCookRunner::FailAllInFlight(const char *reason)
    {
        std::vector<CookJob> pending;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto &[id, flight] : inFlight) {
                pending.push_back(flight.job);
            }
            inFlight.clear();
        }
        for (const auto &job : pending) {
            NotifyCompletion(job.uuid, job.target, AssetBuildRetCode::FAILED, reason);
        }
    }

    void OutOfProcessCookRunner::OnChannelFailure(const char *reason)
    {
        LOG_E(TAG, "channel failure: %s", reason);

        bool first = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            first = !workerFailed;
            workerFailed = true;
            workerReady = false;
        }
        if (first) {
            FailAllInFlight(reason);
        }
    }

} // namespace sky
