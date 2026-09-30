//
// ShaderCompileTask: a self-contained shader compile payload that can be run on
// any thread: inline on the current frame's render parallel workers (blocking,
// not skippable), or on an async worker pool (skippable until Ready()).
//
// The resolver prepares the task (schema / key / cache miss) and owns commit;
// scheduling is the caller's choice.
//

#pragma once

#include <aurora/shader/IShaderCompiler.h>
#include <aurora/shader/ShaderCompile.h>
#include <aurora/shader/ShaderVariant.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sky {
    struct ThreadContext;
    class ThreadPool;
} // namespace sky

namespace sky::aurora {

    class ShaderFileSystem;

    struct ShaderCompileTask {
        ShaderCacheKey           key;
        std::string              source;
        std::string              entry;
        ShaderStageFlagBit       stage   = ShaderStageFlagBit::FS;
        ShaderTarget             target  = ShaderTarget::SPIRV;
        const ShaderVariant     *variant = nullptr; // caller-owned material variant
        ShaderVariantSchema      schema;            // owned
        ShaderFileSystem        *fileSystem = nullptr;
        std::string              relativePath;
        std::vector<std::string> deps;
        uint64_t                 schemaFp   = 0;
        uint64_t                 sourceHash = 0;

        ShaderCompileDesc MakeDesc() const
        {
            ShaderCompileDesc desc;
            desc.source     = source;
            desc.entry      = entry;
            desc.stage      = stage;
            desc.target     = target;
            desc.fileSystem = fileSystem;
            desc.variant    = variant;
            desc.schema     = &schema;
            return desc;
        }

        bool Run(IShaderCompiler &compiler, ShaderCompileResult &out, std::string *error = nullptr) const
        {
            if (!compiler.Compile(MakeDesc(), out)) {
                if (error != nullptr) {
                    *error = out.errorInfo;
                }
                return false;
            }
            return true;
        }
    };

    // Owns a prepared task + compiler + commit callback; run inline or async.
    class ShaderCompileFuture {
    public:
        ShaderCompileFuture() = default;

        void RunInline()
        {
            Run();
        }

        void RunAsync(sky::ThreadPool &pool);

        bool Ready() const
        {
            return mState != nullptr && mState->done.load(std::memory_order_acquire);
        }
        bool Succeeded() const
        {
            return Ready() && mState->ok;
        }
        const ShaderCompileResult &Result() const
        {
            return mState->result;
        }
        const ShaderCompileTask &Task() const
        {
            return mState->task;
        }

    private:
        friend class ShaderResolver;

        struct State {
            ShaderCompileTask                                                           task;
            IShaderCompiler                                                            *compiler = nullptr;
            std::function<void(const ShaderCompileTask &, const ShaderCompileResult &)> commit;
            std::atomic<bool>                                                           done{false};
            bool                                                                        ok = false;
            ShaderCompileResult                                                         result;
        };

        void Run()
        {
            const auto state = mState;
            state->ok        = state->task.Run(*state->compiler, state->result);
            if (state->ok && state->commit) {
                state->commit(state->task, state->result);
            }
            state->done.store(true, std::memory_order_release);
        }

        std::shared_ptr<State> mState;
    };

} // namespace sky::aurora
