//
// Created by Zach Lee on 2022/8/29.
//

#pragma once

#include <thread>
#include <functional>
#include <condition_variable>

namespace sky {

    class WorkThread {
    public:
        WorkThread() = default;
        ~WorkThread();

        void Start();

        void Notify();

        void MainLoop();

    protected:
        void Stop();

        virtual void Execute() = 0;
        std::mutex mutex;
        std::condition_variable cv;
        std::unique_ptr<std::thread> thread;
        std::atomic_bool exit{false};
    };

}