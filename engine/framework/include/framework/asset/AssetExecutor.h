//
// Created by blues on 2024/6/29.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/file/FileSystem.h>
#include <core/async/ThreadPool.h>

#include <framework/asset/Asset.h>

#include <algorithm>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace sky {

    struct SavingTask {
        std::string key;
        std::future<void> asyncTask;
    };

    class AssetExecutor : public Singleton<AssetExecutor> {
    public:
        AssetExecutor();
        ~AssetExecutor() override = default;

        // Schedule func to run after all deps complete. Returns the task node and an already-created future.
        template <typename Func>
        std::pair<TaskNodePtr, std::future<void>> DependentAsync(Func &&func, const std::vector<TaskNodePtr> &deps)
        {
            auto holder = std::make_shared<std::decay_t<Func>>(std::forward<Func>(func));
            auto node = pool.CreateTask([holder](ThreadContext &) mutable { (*holder)(); });
            for (const auto &dep : deps) {
                if (dep) {
                    node->DependsOn(dep);
                }
            }
            // Obtain the future before submitting so completion cannot race the promise creation.
            auto future = node->GetFuture();
            pool.Submit(node);
            return { node, std::move(future) };
        }

        template <typename Func>
        std::pair<TaskNodePtr, std::future<void>> DependentAsync(Func &&func)
        {
            return DependentAsync(std::forward<Func>(func), std::vector<TaskNodePtr>{});
        }

        // Schedule func on `targetPool`, deduped on `key` and tracked by WaitForAll.
        template <typename Func>
        void PushTask(const std::string &key, ThreadPool &targetPool, Func &&func)
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto iter = std::find_if(savingTasks.begin(), savingTasks.end(), [&key](const auto &v) {
                return v.key == key;
            });
            if (iter != savingTasks.end()) {
                return;
            }

            auto holder = std::make_shared<std::decay_t<Func>>(std::forward<Func>(func));
            auto sharedKey = std::make_shared<std::string>(key);
            SavingTask task;
            task.key = key;
            task.asyncTask = targetPool.Dispatch([holder, sharedKey, this](ThreadContext &) mutable {
                (*holder)();
                std::lock_guard<std::mutex> innerLock(mutex);
                auto it = std::find_if(savingTasks.begin(), savingTasks.end(), [&sharedKey](const auto &v) {
                    return v.key == *sharedKey;
                });
                if (it != savingTasks.end()) {
                    savingTasks.erase(it);
                }
            });
            savingTasks.emplace_back(std::move(task));
        }

        // Non-cook save work -> loader pool.
        template <typename Func>
        void PushSavingTask(const std::string &key, Func &&func)
        {
            PushTask(key, pool, std::forward<Func>(func));
        }

        // Cook build -> dedicated cook pool, so the loader pool is never occupied by a cook.
        template <typename Func>
        void PushCookTask(const std::string &key, Func &&func)
        {
            PushTask(key, cookPool, std::forward<Func>(func));
        }

        void WaitForAll();

    private:
        ThreadPool pool;
        ThreadPool cookPool;

        mutable std::mutex mutex;
        std::list<SavingTask> savingTasks;
    };

} // namespace sky
