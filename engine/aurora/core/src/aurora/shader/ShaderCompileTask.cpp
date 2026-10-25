//
// ShaderCompileTask: async scheduling of a prepared compile task.
//

#include <aurora/shader/ShaderCompileTask.h>

#include <core/async/ThreadPool.h>

namespace sky::aurora {

    void ShaderCompileFuture::RunAsync(sky::ThreadPool &pool)
    {
        const auto state = mState;
        if (!state || state->started.exchange(true, std::memory_order_acq_rel))
            return;
        pool.Schedule(sky::ThreadTask([state](sky::ThreadContext &) { Execute(state); }));
    }

} // namespace sky::aurora
