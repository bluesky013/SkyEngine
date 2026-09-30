//
// ShaderCompileTask: async scheduling of a prepared compile task.
//

#include <aurora/shader/ShaderCompileTask.h>

#include <core/async/ThreadPool.h>

namespace sky::aurora {

    void ShaderCompileFuture::RunAsync(sky::ThreadPool &pool)
    {
        pool.Schedule(sky::ThreadTask([state = mState](sky::ThreadContext &) {
            state->ok = state->task.Run(*state->compiler, state->result);
            if (state->ok && state->commit) {
                state->commit(state->task, state->result);
            }
            state->done.store(true, std::memory_order_release);
        }));
    }

} // namespace sky::aurora
