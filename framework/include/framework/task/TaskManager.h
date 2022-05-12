//
// Created by Zach Lee on 2022/3/5.
//

#pragma once
#include <core/environment/Singleton.h>
#include <taskflow/taskflow.hpp>

namespace sky {

    using Task = tf::Task;
    using Executor = tf::Executor;
    using Future = tf::Future<void>;

    class TaskFlow {
    public:
        TaskFlow() = default;
        ~TaskFlow() = default;

        template <typename T>
        Task Emplace(T&& t)
        {
            return taskFlow.emplace(std::forward<T>(t));
        }

    private:
        friend class TaskManager;
        tf::Taskflow taskFlow;
    };


    class TaskManager : public Singleton<TaskManager> {
    public:
        void Setup();
        void Shutdown();

        Future Execute(TaskFlow& flow);

    private:
        friend class Singleton<TaskManager>;
        TaskManager() {}
        ~TaskManager() {}

        Executor executor;
    };
}