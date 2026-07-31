//
// Created by Zach Lee on 2026/3/29.
//

#include <aurora/rhi/Device.h>

namespace sky::aurora {

    void Device::Shutdown()
    {
        WaitIdle();

        mainContext->OnDetach();
        mainContext = nullptr;
    }

    bool Device::Init()
    {
        DeviceInit devInit = {
        };
        if (!OnInit(devInit)) {
            return false;
        }

        mainContext.reset(CreateAsyncContext(QueueType::GRAPHICS));
        mainContext->OnAttach(~(0U));

        UpdateDeviceCaps();

        uint32_t hwConcurrency = std::max(std::thread::hardware_concurrency(), 1U);
        uint32_t threadCount = std::max(1U, hwConcurrency - 1U); // leave one thread for main
        threadCount = std::min(threadCount, capability.maxThreads);
        contexts.resize(threadCount);
        threadPool = std::make_unique<ThreadPool>(threadCount, [this](uint32_t threadIndex) {
            auto *context = CreateAsyncContext(QueueType::GRAPHICS);
            contexts[threadIndex] = context;
            return context;
        });
        threadPool->WaitIdle();

        return true;
    }

} // namespace sky::aurora