//
// Created by blues on 2024/6/29.
//

#pragma once

#include <taskflow/taskflow.hpp>
#include <core/environment/Singleton.h>
#include <core/file/FileSystem.h>

#include <framework/asset/Asset.h>

#include <unordered_map>

namespace sky {

    struct SavingTask {
        FilePtr file;
        tf::AsyncTask asyncTask;
    };

    class AssetExecutor : public Singleton<AssetExecutor> {
    public:
        AssetExecutor() = default;
        ~AssetExecutor() override = default;

        template <typename Func, typename ...Tasks>
        auto DependentAsync(Func &&func, Tasks &&...tasks)
        {
            return executor.dependent_async(std::forward<Func>(func), std::forward<Tasks>(tasks)...);
        }

        template <typename Func, typename Iter>
        auto DependentAsyncRange(Func &&func, Iter begin, Iter last)
        {
            return executor.dependent_async(std::forward<Func>(func), begin, last);
        }

        template <typename Func, typename ...Tasks>
        void PushSavingTask(const FilePtr &file, Func &&func, Tasks &&...tasks)
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto iter  = std::find_if(savingTasks.begin(), savingTasks.end(), [file](const auto &v) {
                return v.file.Get() == file.Get();
            });

            if (iter == savingTasks.end()) {
                SavingTask task = {
                    file,
                    executor.silent_dependent_async([fn = std::forward<Func>(func), file, this]() {
                        fn();
                        std::lock_guard<std::mutex> lock(mutex);
                        auto iter  = std::find_if(savingTasks.begin(), savingTasks.end(), [file](const auto &v) {
                            return v.file.Get() == file.Get();
                        });
                        if (iter != savingTasks.end()) {
                            savingTasks.erase(iter);
                        }
                    }, std::forward<Tasks>(tasks)...)
                };

                savingTasks.emplace_back(std::move(task));
            }
        }

        void WaitForAll();

    private:
        tf::Executor executor;

        mutable std::mutex mutex;
        std::list<SavingTask> savingTasks;
    };

} // namespace sky