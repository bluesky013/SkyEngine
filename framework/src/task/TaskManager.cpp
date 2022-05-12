//
// Created by Zach Lee on 2022/3/5.
//

#include <framework/task/TaskManager.h>
#include <taskflow/taskflow.hpp>

namespace sky {

    void TaskManager::Setup()
    {

    }

    void TaskManager::Shutdown()
    {

    }

    Future TaskManager::Execute(TaskFlow& flow)
    {
        return executor.run(flow.taskFlow);
    }

}