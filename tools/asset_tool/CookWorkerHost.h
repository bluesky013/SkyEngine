//
// Created by blues on 2026/10/3.
//

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <core/util/Uuid.h>

namespace sky {

    class FrameChannel;

    // Worker-side host. Reads cook requests as length-prefixed frames on stdin and writes
    // results on the preserved stdout; all engine logs go to stderr.
    class CookWorkerHost {
    public:
        CookWorkerHost();
        ~CookWorkerHost();

        CookWorkerHost(const CookWorkerHost &) = delete;
        CookWorkerHost &operator=(const CookWorkerHost &) = delete;

        // Blocks until the parent sends Shutdown or the channel fails.
        bool Run();

    private:
        void HandleFrame(const uint8_t *data, size_t size);
        void SendReady();
        void SendResult(uint64_t id, const Uuid &uuid, const std::string &target, int32_t retCode, const std::string &error);

        std::unique_ptr<FrameChannel> channel;
        std::atomic<bool>             quit{false};
    };

} // namespace sky
