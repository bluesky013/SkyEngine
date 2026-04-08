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

        threadPool->WaitIdle();
        threadPool = nullptr;
    }

    bool Device::Init()
    {
        uint32_t hwConcurrency = std::max(std::thread::hardware_concurrency(), 1U);
        uint32_t threadCount = std::max(1U, hwConcurrency - 1U); // leave one thread for main

        DeviceInit devInit = {
            .parallelContextNum = threadCount
        };

        if (!OnInit(devInit)) {
            return false;
        }

        threadCount = std::min(threadCount, capability.maxThreads);

        mainContext.reset(CreateAsyncContext());
        mainContext->OnAttach(~(0U));

        UpdateDeviceCaps();

        contexts.resize(threadCount);
        threadPool = std::make_unique<ThreadPool>(threadCount, [this](uint32_t threadIndex) {
            auto *context = CreateAsyncContext();
            contexts[threadIndex] = context;
            return context;
        });
        threadPool->WaitIdle();

        return true;
    }

} // namespace sky::aurora