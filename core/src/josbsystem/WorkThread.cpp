//
// Created by Zach Lee on 2022/8/29.
//

#include <core/jobsystem/WorkThread.h>

namespace sky {

    WorkThread::~WorkThread()
    {
        Stop();
    }

    void WorkThread::Start()
    {
        thread = std::make_unique<std::thread>(&WorkThread::MainLoop, this);
    }

    void WorkThread::Notify()
    {
        std::lock_guard<std::mutex> lock(mutex);
        cv.notify_all();
    }

    void WorkThread::Stop()
    {
        exit.store(true);
        Notify();
        if (thread->joinable()) {
            thread->join();
        }
    }

    void WorkThread::MainLoop()
    {
        do {
            Execute();
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock);
            }
        } while (!exit);
    }
}