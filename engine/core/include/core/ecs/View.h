//
// ECS view: multi-pool intersection iteration, smallest pool drives.
// Callers never see the storage layout; the view API is stable across
// future storage evolution (sparse-set intersection -> owning groups).
//

#pragma once

#include <core/ecs/EntityRegistry.h>

#include <tuple>
#include <utility>

namespace sky {

    template <typename... Ts>
    class EcsView {
    public:
        explicit EcsView(std::tuple<SparseSet<Ts> &...> pools) : mPools(pools) {}

        template <typename Fn>
        void ForEach(Fn &&fn)
        {
            Dispatch(std::make_index_sequence<sizeof...(Ts)>{}, fn);
        }

    private:
        std::tuple<SparseSet<Ts> &...> mPools;

        // pick the smallest pool as the iteration driver at runtime
        template <size_t... I, typename Fn>
        void Dispatch(std::index_sequence<I...>, Fn &fn)
        {
            const size_t sizes[] = {static_cast<size_t>(std::get<I>(mPools).Size())...};
            size_t driver = 0;
            for (size_t k = 1; k < sizeof...(Ts); ++k) {
                if (sizes[k] < sizes[driver]) {
                    driver = k;
                }
            }
            (..., (I == driver ? IterateDriver<I>(fn, std::make_index_sequence<sizeof...(Ts)>{}) : void()));
        }

        template <size_t D, size_t... I, typename Fn>
        void IterateDriver(Fn &fn, std::index_sequence<I...>)
        {
            auto &driverPool = std::get<D>(mPools);
            for (uint32_t i = 0; i < driverPool.Size(); ++i) {
                const EntityId id = driverPool.DenseEntity(i);
                if ((std::get<I>(mPools).Contains(id) && ...)) {
                    fn(id, RefOf<D, I>(id, i)...);
                }
            }
        }

        template <size_t D, size_t J>
        auto &RefOf(EntityId id, uint32_t driverIndex)
        {
            if constexpr (J == D) {
                (void)id;
                return std::get<J>(mPools).Data(driverIndex);
            } else {
                (void)driverIndex;
                return *std::get<J>(mPools).Get(id);
            }
        }
    };

    template <typename... Ts>
    EcsView<Ts...> MakeView(EntityRegistry &registry)
    {
        return EcsView<Ts...>(std::tie(registry.Pool<Ts>()...));
    }

    template <typename... Ts>
    auto EntityRegistry::View()
    {
        return MakeView<Ts...>(*this);
    }

} // namespace sky
