//
// Cross-thread callback dispatch: one queue per thread role (see editor-cook-async).
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/platform/Platform.h>

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace sky {

    // Thread roles that own a frame/iteration loop. `Main` is the application loop; render / device
    // roles are dispatched by their own loops. Extendable: adding a role adds an enum value.
    enum class ThreadRole : uint32_t {
        Main = 0,
        Render,
        Device,
        Count,
    };

    // Process-wide (cross-DLL) dispatcher: any thread may Post(role, cb); the thread that owns a role
    // calls Dispatch(role) in its loop to run that role's collected callbacks. Callbacks never run
    // under the lock, so a callback may Post again (reentrant) and a slow callback does not block
    // producers. One mutex per role, so roles do not contend.
    class ThreadDispatcher : public Singleton<ThreadDispatcher> {
        friend class Singleton<ThreadDispatcher>;

    public:
        using Callback = std::function<void()>;

        void Post(ThreadRole role, Callback callback)
        {
            Queue                      &queue = queues[static_cast<size_t>(role)];
            std::lock_guard<std::mutex> lock(queue.mutex);
            queue.pending.emplace_back(std::move(callback));
        }

        void PostToMain(Callback callback)
        {
            Post(ThreadRole::Main, std::move(callback));
        }

        // Owner-thread only: swap the queue to a local batch (O(1)) and run outside the lock.
        void Dispatch(ThreadRole role)
        {
            const size_t     index = static_cast<size_t>(role);
            std::thread::id &owner = ownerThread[index];
            if (owner == std::thread::id{}) {
                owner = std::this_thread::get_id();
            }
            SKY_ASSERT(owner == std::this_thread::get_id() && "Dispatch must run on the role's owner thread");

            Queue                 &queue = queues[index];
            std::vector<Callback> batch;
            {
                std::lock_guard<std::mutex> lock(queue.mutex);
                batch.swap(queue.pending);
            }
            for (auto &callback : batch) {
                callback();
            }
        }

        // Shutdown: drop all queued callbacks. Call only after workers are joined (no further Post).
        void Clear()
        {
            for (auto &queue : queues) {
                std::lock_guard<std::mutex> lock(queue.mutex);
                queue.pending.clear();
            }
        }

    private:
        struct Queue {
            std::mutex            mutex;
            std::vector<Callback> pending;
        };

        std::array<Queue, static_cast<size_t>(ThreadRole::Count)>           queues;
        std::array<std::thread::id, static_cast<size_t>(ThreadRole::Count)> ownerThread{};
    };

} // namespace sky
