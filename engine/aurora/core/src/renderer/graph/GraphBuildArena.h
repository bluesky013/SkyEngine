#pragma once

#include <aurora/rdg/FrameExecutionTypes.h>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace sky::aurora {
    class GraphBuildArena final {
    public:
        explicit GraphBuildArena(size_t capacity = 64 * 1024) : capacity(capacity), state(std::make_shared<Storage>(capacity))
        {
        }
        GraphBuildArena(const GraphBuildArena &)            = delete;
        GraphBuildArena &operator=(const GraphBuildArena &) = delete;
        template <class T, class... Args>
        std::variant<FrameError, T *> Make(Args &&...args)
        {
            if (!state || state.use_count() != 1)
                return FrameError{FrameErrorCode::INVALID_STATE, "Build arena has active borrowers"};
            if (sizeof(T) > state->bytes.size() - state->cursor)
                return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Build arena budget exhausted"};
            void  *address   = state->bytes.data() + state->cursor;
            size_t remaining = state->bytes.size() - state->cursor;
            if (!std::align(alignof(T), sizeof(T), address, remaining))
                return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Build arena budget exhausted"};
            auto *object  = new (address) T(std::forward<Args>(args)...);
            state->cursor = static_cast<std::byte *>(address) - state->bytes.data() + sizeof(T);
            state->destructors.push_back({object, [](void *value) { static_cast<T *>(value)->~T(); }});
            return object;
        }
        std::shared_ptr<const void> Borrow() const noexcept
        {
            return state;
        }
        std::shared_ptr<const void> Seal() noexcept
        {
            if (!state)
                return {};
            retired = state;
            return std::exchange(state, {});
        }
        FrameOperationResult Reset()
        {
            if (!retired.expired() || (state && state.use_count() != 1))
                return FrameError{FrameErrorCode::INVALID_STATE, "Build arena has active borrowers"};
            if (state) {
                state->Destroy();
                state = std::make_shared<Storage>(std::move(state->bytes));
            } else {
                state = std::make_shared<Storage>(capacity);
            }
            return FrameOperationComplete{};
        }

    private:
        struct Storage {
            explicit Storage(size_t capacity) : bytes(capacity)
            {
            }
            explicit Storage(std::vector<std::byte> bytes) : bytes(std::move(bytes))
            {
            }
            ~Storage()
            {
                Destroy();
            }
            void Destroy()
            {
                for (auto it = destructors.rbegin(); it != destructors.rend(); ++it)
                    it->destroy(it->object);
                destructors.clear();
            }
            struct Destructor {
                void *object;
                void (*destroy)(void *);
            };
            std::vector<std::byte>  bytes;
            std::vector<Destructor> destructors;
            size_t                  cursor = 0;
        };
        // A borrow seals this generation until all graph and worker owners release it.
        const size_t             capacity;
        std::shared_ptr<Storage> state;
        std::weak_ptr<Storage>   retired;
    };
} // namespace sky::aurora
