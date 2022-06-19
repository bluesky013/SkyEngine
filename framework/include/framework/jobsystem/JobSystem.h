//
// Created by Zach Lee on 2022/6/21.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Macros.h>
#include <taskflow/taskflow.hpp>

namespace sky {

    class JobSystem : public Singleton<JobSystem> {
    public:

        void RunAndWait(tf::Taskflow& flow)
        {
            executor.run(flow).wait();
        }

        auto Run(tf::Taskflow& flow)
        {
            return executor.run(flow);
        }

    private:
        friend class Singleton<JobSystem>;
        JobSystem() = default;
        ~JobSystem() = default;

        tf::Executor executor;
    };

}